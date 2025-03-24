use std::collections::HashMap;
use std::time::{SystemTime, UNIX_EPOCH};

use aws_sdk_codebuild::{types::BuildBatchFilter, Client};
use lambda_runtime::{service_fn, Error, LambdaEvent};
use serde_json::{json, Value};
use tokio_stream::StreamExt;

#[tokio::main]
async fn main() -> Result<(), Error> {
    env_logger::init();
    let func = service_fn(handle);
    lambda_runtime::run(func).await?;
    Ok(())
}

async fn handle(_event: LambdaEvent<Value>) -> Result<(), Error> {
    let region_provider =
        aws_config::meta::region::RegionProviderChain::default_provider().or_else("us-west-2");

    let config = aws_config::from_env().region(region_provider).load().await;

    let sm_client = aws_sdk_secretsmanager::Client::new(&config);

    let github_token = retrieve_secret(
        &sm_client,
        std::env::var("GITHUB_TOKEN_SECRET_NAME")
            .map_err(|_| "failed to find github access token secret name")?,
    )
    .await?;

    let github = octocrab::initialise(octocrab::Octocrab::builder().personal_token(github_token))
        .map_err(|e| format!("failed to build github client: {e}"))?;

    let codebuild_client = Client::new(&config);

    let project = std::env::var("CODEBUILD_PROJECT_NAME").unwrap();

    let github_repo_owner = std::env::var("GITHUB_REPO_OWNER").unwrap();

    let mut pull_requests: HashMap<u64, Vec<CommitBuild>> = HashMap::new();

    log::info!("Pulling builds for {project}");

    let builds = get_project_build_batches(&codebuild_client, project.clone())
        .await
        .unwrap();

    let project_pull_requests = gather_pull_request_builds(&codebuild_client, builds).await?;

    for (k, v) in project_pull_requests {
        let mut builds = v;
        pull_requests
            .entry(k)
            .and_modify(|ev| {
                ev.append(&mut builds);
                ev.dedup();
            })
            .or_insert(builds);
    }

    let mut stopped_builds: u64 = 0;

    for (k, v) in pull_requests.iter() {
        if v.len() <= 1 {
            continue;
        }
        let pull = github
            .pulls(&github_repo_owner, "aws-lc")
            .get(*k)
            .await
            .map_err(|e| format!("failed to retrieve GitHub pull requests: {}", e))?;
        let commit: String = pull.head.sha;
        for cb in v.iter() {
            let build_id = cb.build();
            if cb.commit() == commit {
                log::info!("{build_id} pr/{k} at current head({commit})");
                continue;
            }
            let old_commit = cb.commit();
            log::info!("{build_id} pr/{k} at old head({old_commit}) will be canceled");
            codebuild_client
                .stop_build_batch()
                .set_id(Some(String::from(build_id)))
                .send()
                .await
                .map_err(|e| format!("failed to stop_build_batch: {}", e))?;
            stopped_builds += 1;
        }
    }

    let timestamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_millis();

    println!(
        "{}",
        json!({
            "_aws": {
                "CloudWatchMetrics": [{
                    "Namespace": "AWS-LC",
                    "Dimensions": [
                        ["Project"]
                    ],
                    "Metrics": [
                        {
                            "Name": "PrunedGitHubBuilds",
                            "Unit": "Count",
                            "StorageResolution": 60
                        }
                    ]
                }],
                "Timestamp": timestamp
            },
            "Project": &project,
            "PrunedGitHubBuilds": stopped_builds
        })
        .to_string()
    );

    Ok(())
}

async fn retrieve_secret(
    client: &aws_sdk_secretsmanager::Client,
    secret_id: String,
) -> Result<String, String> {
    let github_secret_output = client
        .get_secret_value()
        .set_secret_id(Some(secret_id))
        .send()
        .await
        .map_err(|e| format!("failed to retrieve GitHub secret: {e}"))?;

    if github_secret_output.secret_string().is_none() {
        return Err("retrieved secret did not have content".to_string());
    }

    Ok(String::from(github_secret_output.secret_string().unwrap()))
}

#[derive(PartialEq, Eq)]
struct CommitBuild(String, String);

impl CommitBuild {
    fn new(commit: String, build: String) -> Self {
        Self(commit, build)
    }

    fn commit(&self) -> &str {
        self.0.as_str()
    }

    fn build(&self) -> &str {
        self.1.as_str()
    }
}

async fn gather_pull_request_builds(
    client: &Client,
    builds: Vec<String>,
) -> Result<HashMap<u64, Vec<CommitBuild>>, String> {
    let mut pull_requests: HashMap<u64, Vec<CommitBuild>> = HashMap::new();

    for chunk in builds.chunks(100) {
        if chunk.is_empty() {
            break;
        }

        let batch = client
            .batch_get_build_batches()
            .set_ids(Some(Vec::from(chunk)))
            .send()
            .await;
        if batch.is_err() {
            return Err(format!(
                "failed to get batch details: {}",
                batch.unwrap_err()
            ));
        }

        for bb in batch.unwrap().build_batches().unwrap_or_default() {
            if bb.source().is_none() {
                continue;
            }
            let source = bb.source().unwrap();
            if source.r#type().is_none()
                || source.r#type().unwrap().as_str()
                    != aws_sdk_codebuild::types::SourceType::Github.as_str()
                || bb.source_version().is_none()
                || bb.resolved_source_version().is_none()
            {
                continue;
            }

            let source_version = bb.source_version().unwrap_or("");

            if !source_version.contains("pr/") {
                continue;
            }

            let pr_number = String::from(source_version).replace("pr/", "");
            let commit = String::from(bb.resolved_source_version().unwrap_or_default());
            let build_id = String::from(bb.id().unwrap_or_default());

            if pr_number.is_empty() || commit.is_empty() || build_id.is_empty() {
                continue;
            }

            pull_requests
                .entry(pr_number.parse::<u64>().unwrap())
                .and_modify(|e| e.push(CommitBuild::new(commit.clone(), build_id.clone())))
                .or_insert(vec![CommitBuild::new(commit, build_id)]);
        }
    }

    for (_, v) in pull_requests.iter_mut() {
        v.dedup();
    }

    Ok(pull_requests)
}

async fn get_project_build_batches(
    client: &Client,
    project: String,
) -> Result<Vec<String>, String> {
    let mut builds: Vec<String> = vec![];

    let mut paginator = client
        .list_build_batches_for_project()
        .set_project_name(Some(project))
        .set_filter(Some(
            BuildBatchFilter::builder()
                .set_status(Some(aws_sdk_codebuild::types::StatusType::InProgress))
                .build(),
        ))
        .into_paginator()
        .send();

    while let Some(result) = paginator.next().await {
        if result.is_err() {
            return Err(format!(
                "failed to list_build_batches_for_project: {}",
                result.unwrap_err()
            ));
        }

        let mut ids = Vec::from(result.unwrap().ids().unwrap_or(&[]));

        builds.append(&mut ids);
    }

    Ok(builds)
}
