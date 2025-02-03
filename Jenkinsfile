import groovy.json.JsonOutput


timestamps {
  node('regular-memory-node') {
    stage('checkout') {
      scmInfo = checkout scm
      println("${scmInfo}")
      env.GIT_BRANCH = scmInfo.GIT_BRANCH
      env.GIT_COMMIT = scmInfo.GIT_COMMIT
    }
    params = [
      string(name: 'GIT_COMMIT', value: scmInfo.GIT_COMMIT),
      string(name: 'GIT_BRANCH', value: scmInfo.GIT_BRANCH),
      string(name: 'parent_job', value: env.JOB_NAME),
      string(name: 'parent_build_number', value: env.BUILD_NUMBER)
    ]
    stage('Build and Test') {
      def jobs = [
        'Linux' : { build job: 'LibSfClient-Linux-Release', parameters: params },
        'Linux-aarch64' : { build job: 'LibSfClient-Linux-aarch64-Release', parameters: params },
        'Win32-VS17' : { build job: 'LibSfClient-Win32-VS17-Release', parameters: params },
        'Win64-VS17' : { build job: 'LibSfClient-Win64-VS17-Release', parameters: params },
        'Macaarch64' : { build job: 'LibSfClient-Macaarch64-Universal-Release', parameters: params }
      ]
      parallel jobs
    }
  }
}
