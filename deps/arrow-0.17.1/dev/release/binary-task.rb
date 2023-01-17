# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

require "digest/sha2"
require "io/console"
require "json"
require "net/http"
require "pathname"
require "tempfile"
require "thread"
require "time"

class BinaryTask
  include Rake::DSL

  class ThreadPool
    def initialize(use_case, &worker)
      @n_workers = choose_n_workers(use_case)
      @worker = worker
      @jobs = Thread::Queue.new
      @workers = @n_workers.times.collect do
        Thread.new do
          loop do
            job = @jobs.pop
            break if job.nil?
            @worker.call(job)
          end
        end
      end
    end

    def <<(job)
      @jobs << job
    end

    def join
      @n_workers.times do
        @jobs << nil
      end
      @workers.each(&:join)
    end

    private
    def choose_n_workers(use_case)
      case use_case
      when :bintray
        # Too many workers cause Bintray error.
        6
      when :gpg
        # Too many workers cause gpg-agent error.
        2
      else
        raise "Unknown use case: #{use_case}"
      end
    end
  end

  class ProgressReporter
    def initialize(label, count_max=0)
      @label = label
      @count_max = count_max

      @mutex = Thread::Mutex.new

      @time_start = Time.now
      @time_previous = Time.now
      @count_current = 0
      @count_previous = 0
    end

    def advance
      @mutex.synchronize do
        @count_current += 1

        return if @count_max.zero?

        time_current = Time.now
        if time_current - @time_previous <= 1
          return
        end

        show_progress(time_current)
      end
    end

    def increment_max
      @mutex.synchronize do
        @count_max += 1
      end
    end

    def finish
      @mutex.synchronize do
        return if @count_max.zero?
        show_progress(Time.now)
        $stderr.puts
      end
    end

    private
    def show_progress(time_current)
      n_finishes = @count_current - @count_previous
      throughput = n_finishes.to_f / (time_current - @time_previous)
      @time_previous = time_current
      @count_previous = @count_current

      message = build_message(time_current, throughput)
      $stderr.print("\r#{message}") if message
    end

    def build_message(time_current, throughput)
      percent = (@count_current / @count_max.to_f) * 100
      formatted_count = "[%s/%s]" % [
        format_count(@count_current),
        format_count(@count_max),
      ]
      elapsed_second = time_current - @time_start
      if throughput.zero?
        rest_second = 0
      else
        rest_second = (@count_max - @count_current) / throughput
      end
      separator = " - "
      progress = "%5.1f%% %s %s %s %s" % [
        percent,
        formatted_count,
        format_time_interval(elapsed_second),
        format_time_interval(rest_second),
        format_throughput(throughput),
      ]
      label = @label

      width = guess_terminal_width
      return "#{label}#{separator}#{progress}" if width.nil?

      return nil if progress.size > width

      label_width = width - progress.size - separator.size
      if label.size > label_width
        ellipsis = "..."
        shorten_label_width = label_width - ellipsis.size
        if shorten_label_width < 1
          return progress
        else
          label = label[0, shorten_label_width] + ellipsis
        end
      end
      "#{label}#{separator}#{progress}"
    end

    def format_count(count)
      "%d" % count
    end

    def format_time_interval(interval)
      if interval < 60
        "00:00:%02d" % interval
      elsif interval < (60 * 60)
        minute, second = interval.divmod(60)
        "00:%02d:%02d" % [minute, second]
      elsif interval < (60 * 60 * 24)
        minute, second = interval.divmod(60)
        hour, minute = minute.divmod(60)
        "%02d:%02d:%02d" % [hour, minute, second]
      else
        minute, second = interval.divmod(60)
        hour, minute = minute.divmod(60)
        day, hour = hour.divmod(24)
        "%dd %02d:%02d:%02d" % [day, hour, minute, second]
      end
    end

    def format_throughput(throughput)
      "%2d/s" % throughput
    end

    def guess_terminal_width
      guess_terminal_width_from_io ||
        guess_terminal_width_from_command ||
        guess_terminal_width_from_env ||
        80
    end

    def guess_terminal_width_from_io
      if IO.respond_to?(:console) and IO.console
        IO.console.winsize[1]
      elsif $stderr.respond_to?(:winsize)
        begin
          $stderr.winsize[1]
        rescue SystemCallError
          nil
        end
      else
        nil
      end
    end

    def guess_terminal_width_from_command
      IO.pipe do |input, output|
        begin
          pid = spawn("tput", "cols", {:out => output, :err => output})
        rescue SystemCallError
          return nil
        end

        output.close
        _, status = Process.waitpid2(pid)
        return nil unless status.success?

        result = input.read.chomp
        begin
          Integer(result, 10)
        rescue ArgumentError
          nil
        end
      end
    end

    def guess_terminal_width_from_env
      env = ENV["COLUMNS"] || ENV["TERM_WIDTH"]
      return nil if env.nil?

      begin
        Integer(env, 10)
      rescue ArgumentError
        nil
      end
    end
  end

  class BintrayClient
    class Error < StandardError
      attr_reader :request
      attr_reader :response
      def initialize(request, response, message)
        @request = request
        @response = response
        super(message)
      end
    end

    def initialize(options={})
      @options = options
      repository = @options[:repository]
      @subject, @repository = repository.split("/", 2) if repository
      @package = @options[:package]
      @version = @options[:version]
      @user = @options[:user]
      @api_key = @options[:api_key]
    end

    def request(method, headers, *components, &block)
      url = build_request_url(*components)
      http = Net::HTTP.new(url.host, url.port)
      http.set_debug_output($stderr) if ENV["DEBUG"]
      http.use_ssl = true
      http.start do |http|
        request = build_request(method, url, headers, &block)
        http.request(request) do |response|
          case response
          when Net::HTTPSuccess
            return JSON.parse(response.body)
          else
            message = "failed to request: "
            message << "#{url}: #{request.method}: "
            message << "#{response.message} #{response.code}:\n"
            message << response.body
            raise Error.new(request, response, message)
          end
        end
      end
    end

    def repository
      request(:get,
              {},
              "repos",
              @subject,
              @repository)
    end

    def create_repository
      request(:post,
              {},
              "repos",
              @subject,
              @repository) do
        request = {
          "name" => @repository,
          "desc" => "Apache Arrow",
        }
        JSON.generate(request)
      end
    end

    def ensure_repository
      begin
        repository
      rescue Error => error
        case error.response
        when Net::HTTPNotFound
          create_repository
        else
          raise
        end
      end
    end

    def package
      request(:get,
              {},
              "packages",
              @subject,
              @repository,
              @package)
    end

    def package_versions
      begin
        package["versions"]
      rescue Error => error
        case error.response
        when Net::HTTPNotFound
          []
        else
          raise
        end
      end
    end

    def create_package(description)
      request(:post,
              {},
              "packages",
              @subject,
              @repository) do
        request = {
          "name" => @package,
          "desc" => description,
          "licenses" => ["Apache-2.0"],
          "vcs_url" => "https://github.com/apache/arrow.git",
          "website_url" => "https://arrow.apache.org/",
          "issue_tracker_url" => "https://issues.apache.org/jira/browse/ARROW",
          "github_repo" => "apache/arrow",
          "public_download_numbers" => true,
          "public_stats" => true,
        }
        JSON.generate(request)
      end
    end

    def ensure_package(description)
      begin
        package
      rescue Error => error
        case error.response
        when Net::HTTPNotFound
          create_package(description)
        else
          raise
        end
      end
    end

    def create_version(description)
      request(:post,
              {},
              "packages",
              @subject,
              @repository,
              @package,
              "versions") do
        request = {
          "name" => @version,
          "desc" => description,
        }
        JSON.generate(request)
      end
    end

    def ensure_version(version, description)
      return if package["versions"].include?(version)
      create_version(description)
    end

    def files
      request(:get,
              {},
              "packages",
              @subject,
              @repository,
              @package,
              "versions",
              @version,
              "files")
    end

    def upload(path, destination_path)
      sha256 = Digest::SHA256.file(path).hexdigest
      headers = {
        "X-Bintray-Override" => "1",
        "X-Bintray-Package" => @package,
        "X-Bintray-Publish" => "1",
        "X-Bintray-Version" => @version,
        "X-Checksum-Sha2" => sha256,
        "Content-Length" => File.size(path).to_s,
      }
      File.open(path, "rb") do |input|
        request(:put,
                headers,
                "content",
                @subject,
                @repository,
                destination_path) do
          input
        end
      end
    end

    def delete(path)
      request(:delete,
              {},
              "content",
              @subject,
              @repository,
              path)
    end

    private
    def build_request_url(*components)
      if components.last.is_a?(Hash)
        parameters = components.pop
      else
        parameters = nil
      end
      path = components.join("/")
      url = "https://bintray.com/api/v1/#{path}"
      if parameters
        separator = "?"
        parameters.each do |key, value|
          url << "#{separator}#{CGI.escape(key)}=#{CGI.escape(value)}"
          separator = "&"
        end
      end
      URI(url)
    end

    def build_request(method, url, headers, &block)
      case method
      when :get
        request = Net::HTTP::Get.new(url, headers)
      when :post
        request = Net::HTTP::Post.new(url, headers)
      when :put
        request = Net::HTTP::Put.new(url, headers)
      when :delete
        request = Net::HTTP::Delete.new(url, headers)
      else
        raise "unsupported HTTP method: #{method.inspect}"
      end
      request.basic_auth(@user, @api_key) if @user and @api_key
      if block_given?
        request["Content-Type"] = "application/json"
        body = yield
        if body.is_a?(String)
          request.body = body
        else
          request.body_stream = body
        end
      end
      request
    end
  end

  module HashChekable
    def same_hash?(path, sha256)
      return false unless File.exist?(path)
      Digest::SHA256.file(path).hexdigest == sha256
    end
  end

  class BintrayDownloader
    include HashChekable

    def initialize(repository:,
                   distribution:,
                   version:,
                   rc: nil,
                   destination:,
                   user:,
                   api_key:)
      @repository = repository
      @distribution = distribution
      @version = version
      @rc = rc
      @destination = destination
      @user = user
      @api_key = api_key
    end

    def download
      client.ensure_repository

      progress_label = "Downloading: #{package} #{full_version}"
      progress_reporter = ProgressReporter.new(progress_label)
      pool = ThreadPool.new(:bintray) do |path, output_path|
        download_file(path, output_path)
        progress_reporter.advance
      end
      target_files.each do |file|
        path = file["path"]
        path_without_package = path.split("/", 2)[1..-1].join("/")
        output_path = "#{@destination}/#{path_without_package}"
        yield(output_path)
        sha256 = file["sha256"]
        next if same_hash?(output_path, sha256)
        output_dir = File.dirname(output_path)
        FileUtils.mkdir_p(output_dir)
        progress_reporter.increment_max
        pool << [path, output_path]
      end
      pool.join
      progress_reporter.finish
    end

    private
    def package
      if @rc
        "#{@distribution}-rc"
      else
        @distribution
      end
    end

    def full_version
      if @rc
        "#{@version}-rc#{@rc}"
      else
        @version
      end
    end

    def client(options={})
      default_options = {
        repository: @repository,
        package: package,
        version: full_version,
        user: @user,
        api_key: @api_key,
      }
      BintrayClient.new(default_options.merge(options))
    end

    def target_files
      begin
        client.files
      rescue BintrayClient::Error
        []
      end
    end

    def download_file(path, output_path)
      max_n_retries = 5
      n_retries = 0
      url = URI("https://dl.bintray.com/#{@repository}/#{path}")
      begin
        download_url(url, output_path)
      rescue SocketError, SystemCallError, Timeout::Error => error
        n_retries += 1
        if n_retries <= max_n_retries
          $stderr.puts
          $stderr.puts("Retry #{n_retries}: #{url}: " +
                       "#{error.class}: #{error.message}")
          retry
        else
          raise
        end
      end
    end

    def download_url(url, output_path)
      loop do
        http = Net::HTTP.new(url.host, url.port)
        http.set_debug_output($stderr) if ENV["DEBUG"]
        http.use_ssl = true
        http.start do |http|
          request = Net::HTTP::Get.new(url)
          http.request(request) do |response|
            case response
            when Net::HTTPSuccess
              save_response(response, output_path)
              return
            when Net::HTTPRedirection
              url = URI(response["Location"])
            when Net::HTTPNotFound
              $stderr.puts(build_download_error_message(url, response))
              return
            else
              raise message
            end
          end
        end
      end
    end

    def save_response(response, output_path)
      File.open(output_path, "wb") do |output|
        response.read_body do |chunk|
          output.print(chunk)
        end
      end
      last_modified = response["Last-Modified"]
      if last_modified
        FileUtils.touch(output_path, mtime: Time.rfc2822(last_modified))
      end
    end

    def build_download_error_message(url, response)
      message = "failed to download: "
      message << "#{url}: #{response.message} #{response.code}:\n"
      message << response.body
      message
    end
  end

  class BintrayUploader
    include HashChekable

    def initialize(repository:,
                   distribution:,
                   distribution_label:,
                   version:,
                   rc: nil,
                   source:,
                   destination_prefix: "",
                   user:,
                   api_key:)
      @repository = repository
      @distribution = distribution
      @distribution_label = distribution_label
      @version = version
      @rc = rc
      @source = source
      @destination_prefix = destination_prefix
      @user = user
      @api_key = api_key
    end

    def upload
      client.ensure_repository
      client.ensure_package(package_description)
      client.ensure_version(full_version, version_description)

      progress_label = "Uploading: #{package} #{full_version}"
      progress_reporter = ProgressReporter.new(progress_label)
      pool = ThreadPool.new(:bintray) do |path, relative_path|
        upload_file(path, relative_path)
        progress_reporter.advance
      end

      files = existing_files
      source = Pathname(@source)
      source.glob("**/*") do |path|
        next if path.directory?
        destination_path =
          "#{package}/#{@destination_prefix}#{path.relative_path_from(source)}"
        file = files[destination_path]
        next if file and same_hash?(path.to_s, file["sha256"])
        progress_reporter.increment_max
        pool << [path, destination_path]
      end
      pool.join
      progress_reporter.finish
    end

    private
    def package
      if @rc
        "#{@distribution}-rc"
      else
        @distribution
      end
    end

    def full_version
      if @rc
        "#{@version}-rc#{@rc}"
      else
        @version
      end
    end

    def package_description
      if @rc
        release_type = "RC"
      else
        release_type = "Release"
      end
      case @distribution
      when "debian", "ubuntu"
        "#{release_type} deb packages for #{@distribution_label}"
      when "centos"
        "#{release_type} RPM packages for #{@distribution_label}"
      else
        "#{release_type} binaries for #{@distribution_label}"
      end
    end

    def version_description
      if @rc
        "Apache Arrow #{@version} RC#{@rc} for #{@distribution_label}"
      else
        "Apache Arrow #{@version} for #{@distribution_label}"
      end
    end

    def client
      BintrayClient.new(repository: @repository,
                        package: package,
                        version: full_version,
                        user: @user,
                        api_key: @api_key)
    end

    def existing_files
      files = {}
      client.files.each do |file|
        files[file["path"]] = file
      end
      files
    end

    def upload_file(path, destination_path)
      max_n_retries = 3
      n_retries = 0
      begin
        begin
          client.upload(path, destination_path)
        rescue BintrayClient::Error => error
          case error.response
          when Net::HTTPConflict
            n_retries += 1
            if n_retries <= max_n_retries
              client.delete(destination_path)
              retry
            else
              $stderr.puts(error)
            end
          else
            $stderr.puts(error)
          end
        end
      rescue SocketError, SystemCallError, Timeout::Error => error
        n_retries += 1
        if n_retries <= max_n_retries
          $stderr.puts
          $stderr.puts("Retry #{n_retries}: #{path}: " +
                       "#{error.class}: #{error.message}")
          retry
        else
          raise
        end
      end
    end
  end

  def define
    define_apt_tasks
    define_yum_tasks
    define_python_tasks
    define_nuget_tasks
    define_summary_tasks
  end

  private
  def env_value(name)
    value = ENV[name]
    value = yield(name) if value.nil? and block_given?
    raise "Specify #{name} environment variable" if value.nil?
    value
  end

  def verbose?
    ENV["VERBOSE"] == "yes"
  end

  def default_output
    if verbose?
      nil
    else
      IO::NULL
    end
  end

  def gpg_key_id
    env_value("GPG_KEY_ID")
  end

  def shorten_gpg_key_id(id)
    id[-8..-1]
  end

  def rpm_gpg_key_package_name(id)
    "gpg-pubkey-#{shorten_gpg_key_id(id).downcase}"
  end

  def bintray_user
    env_value("BINTRAY_USER")
  end

  def bintray_api_key
    env_value("BINTRAY_API_KEY")
  end

  def bintray_repository
    env_value("BINTRAY_REPOSITORY")
  end

  def source_bintray_repository
    env_value("SOURCE_BINTRAY_REPOSITORY") do
      bintray_repository
    end
  end

  def artifacts_dir
    env_value("ARTIFACTS_DIR")
  end

  def version
    env_value("VERSION")
  end

  def rc
    env_value("RC")
  end

  def full_version
    "#{version}-rc#{rc}"
  end

  def valid_sign?(path, sign_path)
    IO.pipe do |input, output|
      begin
        sh({"LANG" => "C"},
           "gpg",
           "--verify",
           sign_path,
           path,
           out: default_output,
           err: output,
           verbose: false)
      rescue
        return false
      end
      output.close
      /Good signature/ === input.read
    end
  end

  def sign(source_path, destination_path)
    if File.exist?(destination_path)
      return if valid_sign?(source_path, destination_path)
      rm(destination_path, verbose: false)
    end
    sh("gpg",
       "--detach-sig",
       "--local-user", gpg_key_id,
       "--output", destination_path,
       source_path,
       out: default_output,
       verbose: verbose?)
  end

  def sha512(source_path, destination_path)
    if File.exist?(destination_path)
      sha512 = File.read(destination_path).split[0]
      return if Digest::SHA512.file(source_path).hexdigest == sha512
    end
    absolute_destination_path = File.expand_path(destination_path)
    Dir.chdir(File.dirname(source_path)) do
      sh("shasum",
         "--algorithm", "512",
         File.basename(source_path),
         out: absolute_destination_path,
         verbose: verbose?)
    end
  end

  def sign_dir(label, dir)
    progress_label = "Signing: #{label}"
    progress_reporter = ProgressReporter.new(progress_label)

    target_paths = []
    Pathname(dir).glob("**/*") do |path|
      next if path.directory?
      case path.extname
      when ".asc", ".sha512"
        next
      end
      progress_reporter.increment_max
      target_paths << path.to_s
    end
    target_paths.each do |path|
      sign(path, "#{path}.asc")
      sha512(path, "#{path}.sha512")
      progress_reporter.advance
    end
    progress_reporter.finish
  end

  def download_distribution(distribution,
                            destination,
                            with_source_repository: false)
    existing_paths = {}
    Pathname(destination).glob("**/*") do |path|
      next if path.directory?
      existing_paths[path.to_s] = true
    end
    if with_source_repository
      source_client = BintrayClient.new(repository: source_bintray_repository,
                                        package: distribution,
                                        user: bintray_user,
                                        api_key: bintray_api_key)
      source_client.package_versions[0, 10].each do |source_version|
        downloader = BintrayDownloader.new(repository: source_bintray_repository,
                                           distribution: distribution,
                                           version: source_version,
                                           destination: destination,
                                           user: bintray_user,
                                           api_key: bintray_api_key)
        downloader.download do |output_path|
          existing_paths.delete(output_path)
        end
      end
    end
    downloader = BintrayDownloader.new(repository: bintray_repository,
                                       distribution: distribution,
                                       version: version,
                                       rc: rc,
                                       destination: destination,
                                       user: bintray_user,
                                       api_key: bintray_api_key)
    downloader.download do |output_path|
      existing_paths.delete(output_path)
    end
    existing_paths.each_key do |path|
      rm_f(path, verbose: verbose?)
    end
  end

  def same_content?(path1, path2)
    File.exist?(path1) and
      File.exist?(path2) and
      Digest::SHA256.file(path1) == Digest::SHA256.file(path2)
  end

  def copy_artifact(source_path,
                    destination_path,
                    progress_reporter)
    return if same_content?(source_path, destination_path)
    progress_reporter.increment_max
    destination_dir = File.dirname(destination_path)
    unless File.exist?(destination_dir)
      mkdir_p(destination_dir, verbose: verbose?)
    end
    cp(source_path, destination_path, verbose: verbose?)
    progress_reporter.advance
  end

  def tmp_dir
    "binary/tmp"
  end

  def rc_dir
    "#{tmp_dir}/rc"
  end

  def release_dir
    "#{tmp_dir}/release"
  end

  def deb_dir
    "#{rc_dir}/deb/#{full_version}"
  end

  def apt_repository_label
    "Apache Arrow"
  end

  def apt_repository_description
    "Apache Arrow packages"
  end

  def apt_rc_repositories_dir
    "#{rc_dir}/apt/repositories"
  end

  def apt_release_repositories_dir
    "#{release_dir}/apt/repositories"
  end

  def available_apt_targets
    [
      ["debian", "stretch", "main"],
      ["debian", "buster", "main"],
      ["ubuntu", "xenial", "main"],
      ["ubuntu", "bionic", "main"],
      ["ubuntu", "eoan", "main"],
      ["ubuntu", "focal", "main"],
    ]
  end

  def apt_distribution_label(distribution)
    case distribution
    when "debian"
      "Debian"
    when "ubuntu"
      "Ubuntu"
    else
      distribution
    end
  end

  def apt_targets
    env_apt_targets = (ENV["APT_TARGETS"] || "").split(",")
    if env_apt_targets.empty?
      available_apt_targets
    else
      available_apt_targets.select do |distribution, code_name, component|
        env_apt_targets.any? do |env_apt_target|
          "#{distribution}-#{code_name}".start_with?(env_apt_target)
        end
      end
    end
  end

  def apt_distributions
    apt_targets.collect(&:first).uniq
  end

  def apt_architectures
    [
      "amd64",
      "arm64",
    ]
  end

  def define_deb_tasks
    directory deb_dir

    namespace :deb do
      desc "Copy deb packages"
      task :copy => deb_dir do
        apt_targets.each do |distribution, code_name, component|
          progress_label = "Copying: #{distribution} #{code_name}"
          progress_reporter = ProgressReporter.new(progress_label)

          source_dir_prefix = "#{artifacts_dir}/#{distribution}-#{code_name}"
          Dir.glob("#{source_dir_prefix}*/**/*") do |path|
            next if File.directory?(path)
            base_name = File.basename(path)
            if base_name.start_with?("apache-arrow-archive-keyring")
              package_name = "apache-arrow-archive-keyring"
            else
              package_name = "apache-arrow"
            end
            distribution_dir = [
              deb_dir,
              distribution,
            ].join("/")
            destination_path = [
              distribution_dir,
              "pool",
              code_name,
              component,
              package_name[0],
              package_name,
              base_name,
            ].join("/")
            copy_artifact(path,
                          destination_path,
                          progress_reporter)
            case base_name
            when /\A[^_]+-archive-keyring_.*\.deb\z/
              latest_archive_keyring_package_path = [
                distribution_dir,
                "#{package_name}-latest-#{code_name}.deb"
              ].join("/")
              copy_artifact(path,
                            latest_archive_keyring_package_path,
                            progress_reporter)
            end
          end
          progress_reporter.finish
        end
      end

      desc "Sign deb packages"
      task :sign => deb_dir do
        apt_distributions.each do |distribution|
          distribution_dir = "#{deb_dir}/#{distribution}"
          Dir.glob("#{distribution_dir}/**/*.dsc") do |path|
            begin
              sh({"LANG" => "C"},
                 "gpg",
                 "--verify",
                 path,
                 out: IO::NULL,
                 err: IO::NULL,
                 verbose: false)
            rescue
              sh("debsign",
                 "--no-re-sign",
                 "-k#{gpg_key_id}",
                 path,
                 out: default_output,
                 verbose: verbose?)
            end
          end
          sign_dir(distribution, distribution_dir)
        end
      end

      desc "Upload deb packages"
      task :upload do
        apt_distributions.each do |distribution|
          distribution_dir = "#{deb_dir}/#{distribution}"
          distribution_label = apt_distribution_label(distribution)
          uploader = BintrayUploader.new(repository: bintray_repository,
                                         distribution: distribution,
                                         distribution_label: distribution_label,
                                         version: version,
                                         rc: rc,
                                         source: distribution_dir,
                                         user: bintray_user,
                                         api_key: bintray_api_key)
          uploader.upload
        end
      end
    end

    desc "Release deb packages"
    deb_tasks = [
      "deb:copy",
      "deb:sign",
      "deb:upload",
    ]
    task :deb => deb_tasks
  end

  def generate_apt_release(dists_dir, code_name, component, architecture)
    dir = "#{dists_dir}/#{component}/"
    if architecture == "source"
      dir << architecture
    else
      dir << "binary-#{architecture}"
    end

    mkdir_p(dir, verbose: verbose?)
    File.open("#{dir}/Release", "w") do |release|
      release.puts(<<-RELEASE)
Archive: #{code_name}
Component: #{component}
Origin: #{apt_repository_label}
Label: #{apt_repository_label}
Architecture: #{architecture}
      RELEASE
    end
  end

  def generate_apt_ftp_archive_generate_conf(code_name, component)
    conf = <<-CONF
Dir::ArchiveDir ".";
Dir::CacheDir ".";
TreeDefault::Directory "pool/#{code_name}/#{component}";
TreeDefault::SrcDirectory "pool/#{code_name}/#{component}";
Default::Packages::Extensions ".deb";
Default::Packages::Compress ". gzip xz";
Default::Sources::Compress ". gzip xz";
Default::Contents::Compress "gzip";
    CONF

    apt_architectures.each do |architecture|
      conf << <<-CONF

BinDirectory "dists/#{code_name}/#{component}/binary-#{architecture}" {
  Packages "dists/#{code_name}/#{component}/binary-#{architecture}/Packages";
  Contents "dists/#{code_name}/#{component}/Contents-#{architecture}";
  SrcPackages "dists/#{code_name}/#{component}/source/Sources";
};
      CONF
    end

    conf << <<-CONF

Tree "dists/#{code_name}" {
  Sections "#{component}";
  Architectures "#{apt_architectures.join(" ")} source";
};
    CONF

    conf
  end

  def generate_apt_ftp_archive_release_conf(code_name, component)
    <<-CONF
APT::FTPArchive::Release::Origin "#{apt_repository_label}";
APT::FTPArchive::Release::Label "#{apt_repository_label}";
APT::FTPArchive::Release::Architectures "#{apt_architectures.join(" ")}";
APT::FTPArchive::Release::Codename "#{code_name}";
APT::FTPArchive::Release::Suite "#{code_name}";
APT::FTPArchive::Release::Components "#{component}";
APT::FTPArchive::Release::Description "#{apt_repository_description}";
    CONF
  end

  def define_apt_rc_tasks
    directory apt_rc_repositories_dir

    namespace :apt do
      namespace :rc do
        desc "Download deb files for RC APT repositories"
        task :download => apt_rc_repositories_dir do
          apt_distributions.each do |distribution|
            download_distribution(distribution,
                                  "#{apt_rc_repositories_dir}/#{distribution}",
                                  with_source_repository: true)
          end
        end

        desc "Update RC APT repositories"
        task :update do
          apt_targets.each do |distribution, code_name, component|
            base_dir = "#{apt_rc_repositories_dir}/#{distribution}"
            pool_dir = "#{base_dir}/pool/#{code_name}"
            next unless File.exist?(pool_dir)
            dists_dir = "#{base_dir}/dists/#{code_name}"
            rm_rf(dists_dir, verbose: verbose?)
            generate_apt_release(dists_dir, code_name, component, "source")
            apt_architectures.each do |architecture|
              generate_apt_release(dists_dir, code_name, component, architecture)
            end

            generate_conf_file = Tempfile.new("apt-ftparchive-generate.conf")
            File.open(generate_conf_file.path, "w") do |conf|
              conf.puts(generate_apt_ftp_archive_generate_conf(code_name,
                                                               component))
            end
            cd(base_dir, verbose: verbose?) do
              sh("apt-ftparchive",
                 "generate",
                 generate_conf_file.path,
                 out: default_output,
                 verbose: verbose?)
            end

            Dir.glob("#{dists_dir}/Release*") do |release|
              rm_f(release, verbose: verbose?)
            end
            Dir.glob("#{base_dir}/*.db") do |db|
              rm_f(db, verbose: verbose?)
            end
            release_conf_file = Tempfile.new("apt-ftparchive-release.conf")
            File.open(release_conf_file.path, "w") do |conf|
              conf.puts(generate_apt_ftp_archive_release_conf(code_name,
                                                              component))
            end
            release_file = Tempfile.new("apt-ftparchive-release")
            sh("apt-ftparchive",
               "-c", release_conf_file.path,
               "release",
               dists_dir,
               out: release_file.path,
               verbose: verbose?)
            release_path = "#{dists_dir}/Release"
            signed_release_path = "#{release_path}.gpg"
            in_release_path = "#{dists_dir}/InRelease"
            mv(release_file.path, release_path, verbose: verbose?)
            chmod(0644, release_path, verbose: verbose?)
            sh("gpg",
               "--sign",
               "--detach-sign",
               "--armor",
               "--local-user", gpg_key_id,
               "--output", signed_release_path,
               release_path,
               out: default_output,
               verbose: verbose?)
            sh("gpg",
               "--clear-sign",
               "--local-user", gpg_key_id,
               "--output", in_release_path,
               release_path,
               out: default_output,
               verbose: verbose?)

            sign_dir("#{distribution} #{code_name}",
                     dists_dir)
          end
        end

        desc "Upload RC APT repositories"
        task :upload => apt_rc_repositories_dir do
          apt_distributions.each do |distribution|
            dists_dir = "#{apt_rc_repositories_dir}/#{distribution}/dists"
            distribution_label = apt_distribution_label(distribution)
            uploader = BintrayUploader.new(repository: bintray_repository,
                                           distribution: distribution,
                                           distribution_label: distribution_label,
                                           version: version,
                                           rc: rc,
                                           source: dists_dir,
                                           destination_prefix: "dists/",
                                           user: bintray_user,
                                           api_key: bintray_api_key)
            uploader.upload
          end
        end
      end

      desc "Release RC APT repositories"
      apt_rc_tasks = [
        "apt:rc:download",
        "apt:rc:update",
        "apt:rc:upload",
      ]
      task :rc => apt_rc_tasks
    end
  end

  def define_apt_release_tasks
    directory apt_release_repositories_dir

    namespace :apt do
      namespace :release do
        desc "Download RC APT repositories"
        task :download => apt_release_repositories_dir do
          apt_distributions.each do |distribution|
            distribution_dir = "#{apt_release_repositories_dir}/#{distribution}"
            download_distribution(distribution, distribution_dir)
          end
        end

        desc "Upload release APT repositories"
        task :upload => apt_release_repositories_dir do
          apt_distributions.each do |distribution|
            distribution_dir = "#{apt_release_repositories_dir}/#{distribution}"
            distribution_label = apt_distribution_label(distribution)
            uploader = BintrayUploader.new(repository: bintray_repository,
                                           distribution: distribution,
                                           distribution_label: distribution_label,
                                           version: version,
                                           source: distribution_dir,
                                           user: bintray_user,
                                           api_key: bintray_api_key)
            uploader.upload
          end
        end
      end

      desc "Release APT repositories"
      apt_release_tasks = [
        "apt:release:download",
        "apt:release:upload",
      ]
      task :release => apt_release_tasks
    end
  end

  def define_apt_tasks
    define_deb_tasks
    define_apt_rc_tasks
    define_apt_release_tasks
  end

  def rpm_dir
    "#{rc_dir}/rpm/#{full_version}"
  end

  def yum_rc_repositories_dir
    "#{rc_dir}/yum/repositories"
  end

  def yum_release_repositories_dir
    "#{release_dir}/yum/repositories"
  end

  def available_yum_targets
    [
      ["centos", "6"],
      ["centos", "7"],
      ["centos", "8"],
    ]
  end

  def yum_distribution_label(distribution)
    case distribution
    when "centos"
      "CentOS"
    else
      distribution
    end
  end

  def yum_targets
    env_yum_targets = (ENV["YUM_TARGETS"] || "").split(",")
    if env_yum_targets.empty?
      available_yum_targets
    else
      available_yum_targets.select do |distribution, distribution_version|
        env_yum_targets.any? do |env_yum_target|
          "#{distribution}-#{distribution_version}".start_with?(env_yum_target)
        end
      end
    end
  end

  def yum_distributions
    yum_targets.collect(&:first).uniq
  end

  def yum_architectures
    [
      "aarch64",
      "x86_64",
    ]
  end

  def signed_rpm?(rpm)
    IO.pipe do |input, output|
      system("rpm", "--checksig", rpm, out: output)
      output.close
      signature = input.gets.sub(/\A#{Regexp.escape(rpm)}: /, "")
      signature.split.include?("signatures")
    end
  end

  def sign_rpms(directory)
    thread_pool = ThreadPool.new(:gpg) do |rpm|
      unless signed_rpm?(rpm)
        sh("rpm",
           "-D", "_gpg_name #{gpg_key_id}",
           "-D", "__gpg_check_password_cmd /bin/true true",
           "--resign",
           rpm,
           out: default_output,
           verbose: verbose?)
      end
    end
    Dir.glob("#{directory}/**/*.rpm") do |rpm|
      thread_pool << rpm
    end
    thread_pool.join
  end

  def define_rpm_tasks
    directory rpm_dir

    namespace :rpm do
      desc "Copy RPM packages"
      task :copy => rpm_dir do
        yum_targets.each do |distribution, distribution_version|
          progress_label = "Copying: #{distribution} #{distribution_version}"
          progress_reporter = ProgressReporter.new(progress_label)

          destination_prefix = [
            rpm_dir,
            distribution,
            distribution_version,
          ].join("/")
          source_dir_prefix =
            "#{artifacts_dir}/#{distribution}-#{distribution_version}"
          Dir.glob("#{source_dir_prefix}*/**/*") do |path|
            next if File.directory?(path)
            base_name = File.basename(path)
            type = base_name.split(".")[-2]
            destination_paths = []
            case type
            when "src"
              destination_paths << [
                destination_prefix,
                "Source",
                "SPackages",
                base_name,
              ].join("/")
            when "noarch"
              yum_architectures.each do |architecture|
                destination_paths << [
                  destination_prefix,
                  architecture,
                  "Packages",
                  base_name,
                ].join("/")
              end
            else
              destination_paths << [
                destination_prefix,
                type,
                "Packages",
                base_name,
              ].join("/")
            end
            destination_paths.each do |destination_path|
              copy_artifact(path,
                            destination_path,
                            progress_reporter)
            end
            case base_name
            when /\A(apache-arrow-release)-.*\.noarch\.rpm\z/
              package_name = $1
              latest_release_package_path = [
                destination_prefix,
                "#{package_name}-latest.rpm"
              ].join("/")
              copy_artifact(path,
                            latest_release_package_path,
                            progress_reporter)
            end
          end

          progress_reporter.finish
        end
      end

      desc "Sign RPM packages"
      task :sign do
        unless system("rpm", "-q",
                      rpm_gpg_key_package_name(gpg_key_id),
                      out: IO::NULL)
          gpg_key = Tempfile.new(["apache-arrow-binary", ".asc"])
          sh("gpg",
             "--armor",
             "--export", gpg_key_id,
             out: gpg_key.path,
             verbose: verbose?)
          sh("rpm",
             "--import", gpg_key.path,
             out: default_output,
             verbose: verbose?)
          gpg_key.close!
        end

        yum_targets.each do |distribution, distribution_version|
          source_dir = [
            rpm_dir,
            distribution,
            distribution_version,
          ].join("/")
          sign_rpms(source_dir)
          sign_dir("#{distribution}-#{distribution_version}",
                   rpm_dir)
        end
      end

      desc "Upload RPM packages"
      task :upload do
        yum_distributions.each do |distribution|
          distribution_dir = "#{rpm_dir}/#{distribution}"
          distribution_label = yum_distribution_label(distribution)
          uploader = BintrayUploader.new(repository: bintray_repository,
                                         distribution: distribution,
                                         distribution_label: distribution_label,
                                         version: version,
                                         rc: rc,
                                         source: distribution_dir,
                                         user: bintray_user,
                                         api_key: bintray_api_key)
          uploader.upload
        end
      end
    end

    desc "Release RPM packages"
    rpm_tasks = [
      "rpm:copy",
      "rpm:sign",
      "rpm:upload",
    ]
    task :rpm => rpm_tasks
  end

  def define_yum_rc_tasks
    directory yum_rc_repositories_dir

    namespace :yum do
      namespace :rc do
        desc "Download RPM files for RC Yum repositories"
        task :download => yum_rc_repositories_dir do
          yum_distributions.each do |distribution|
            distribution_dir = "#{yum_rc_repositories_dir}/#{distribution}"
            download_distribution(distribution,
                                  distribution_dir,
                                  with_source_repository: true)
          end
        end

        desc "Update RC Yum repositories"
        task :update => yum_rc_repositories_dir do
          yum_distributions.each do |distribution|
            distribution_dir = "#{yum_rc_repositories_dir}/#{distribution}"
            Dir.glob("#{distribution_dir}/**/repodata") do |repodata|
              rm_rf(repodata, verbose: verbose?)
            end
          end

          yum_targets.each do |distribution, distribution_version|
            base_dir = [
              yum_rc_repositories_dir,
              distribution,
              distribution_version,
            ].join("/")
            base_dir = Pathname(base_dir)
            next unless base_dir.directory?
            base_dir.glob("*") do |arch_dir|
              next unless arch_dir.directory?
              sh("createrepo",
                 "--update",
                 arch_dir.to_s,
                 out: default_output,
                 verbose: verbose?)
              sign_label =
                "#{distribution}-#{distribution_version} #{arch_dir.basename}"
              sign_dir(sign_label,
                       arch_dir.to_s)
            end
          end
        end

        desc "Upload RC Yum repositories"
        task :upload => yum_rc_repositories_dir do
          yum_targets.each do |distribution, distribution_version|
            distribution_label = yum_distribution_label(distribution)
            base_dir = [
              yum_rc_repositories_dir,
              distribution,
              distribution_version,
            ].join("/")
            base_dir = Pathname(base_dir)
            base_dir.glob("**/repodata") do |repodata_dir|
              relative_dir = [
                distribution_version,
                repodata_dir.relative_path_from(base_dir).to_s
              ].join("/")
              uploader =
                BintrayUploader.new(repository: bintray_repository,
                                    distribution: distribution,
                                    distribution_label: distribution_label,
                                    version: version,
                                    rc: rc,
                                    source: repodata_dir.to_s,
                                    destination_prefix: "#{relative_dir}/",
                                    user: bintray_user,
                                    api_key: bintray_api_key)
              uploader.upload
            end
          end
        end
      end

      desc "Release RC Yum packages"
      yum_rc_tasks = [
        "yum:rc:download",
        "yum:rc:update",
        "yum:rc:upload",
      ]
      task :rc => yum_rc_tasks
    end
  end

  def define_yum_release_tasks
    directory yum_release_repositories_dir

    namespace :yum do
      namespace :release do
        desc "Download RC Yum repositories"
        task :download => yum_release_repositories_dir do
          yum_distributions.each do |distribution|
            distribution_dir = "#{yum_release_repositories_dir}/#{distribution}"
            download_distribution(distribution, distribution_dir)
          end
        end

        desc "Upload release Yum repositories"
        task :upload => yum_release_repositories_dir do
          yum_distributions.each do |distribution|
            distribution_dir = "#{yum_release_repositories_dir}/#{distribution}"
            distribution_label = yum_distribution_label(distribution)
            uploader = BintrayUploader.new(repository: bintray_repository,
                                           distribution: distribution,
                                           distribution_label: distribution_label,
                                           version: version,
                                           source: distribution_dir,
                                           user: bintray_user,
                                           api_key: bintray_api_key)
            uploader.upload
          end
        end
      end

      desc "Release Yum packages"
      yum_release_tasks = [
        "yum:release:download",
        "yum:release:upload",
      ]
      task :release => yum_release_tasks
    end
  end

  def define_yum_tasks
    define_rpm_tasks
    define_yum_rc_tasks
    define_yum_release_tasks
  end

  def define_generic_data_rc_tasks(label,
                                   id,
                                   rc_dir,
                                   target_files_glob)
    directory rc_dir

    namespace id do
      namespace :rc do
        desc "Copy #{label} packages"
        task :copy => rc_dir do
          progress_label = "Copying: #{label}"
          progress_reporter = ProgressReporter.new(progress_label)

          Pathname(artifacts_dir).glob(target_files_glob) do |path|
            next if path.directory?
            destination_path = [
              rc_dir,
              path.basename.to_s,
            ].join("/")
            copy_artifact(path, destination_path, progress_reporter)
          end

          progress_reporter.finish
        end

        desc "Sign #{label} packages"
        task :sign => rc_dir do
          sign_dir(label, rc_dir)
        end

        desc "Upload #{label} packages"
        task :upload do
          uploader = BintrayUploader.new(repository: bintray_repository,
                                         distribution: id.to_s,
                                         distribution_label: label,
                                         version: version,
                                         rc: rc,
                                         source: rc_dir,
                                         destination_prefix: "#{full_version}/",
                                         user: bintray_user,
                                         api_key: bintray_api_key)
          uploader.upload
        end
      end

      desc "Release RC #{label} packages"
      rc_tasks = [
        "#{id}:rc:copy",
        "#{id}:rc:sign",
        "#{id}:rc:upload",
      ]
      task :rc => rc_tasks
    end
  end

  def define_generic_data_release_tasks(label, id, release_dir)
    directory release_dir

    namespace id do
      namespace :release do
        desc "Download RC #{label} packages"
        task :download => release_dir do
          download_distribution(id.to_s, release_dir)
        end

        desc "Upload release #{label} packages"
        task :upload => release_dir do
          packages_dir = "#{release_dir}/#{full_version}"
          uploader = BintrayUploader.new(repository: bintray_repository,
                                         distribution: id.to_s,
                                         distribution_label: label,
                                         version: version,
                                         source: packages_dir,
                                         destination_prefix: "#{version}/",
                                         user: bintray_user,
                                         api_key: bintray_api_key)
          uploader.upload
        end
      end

      desc "Release #{label} packages"
      release_tasks = [
        "#{id}:release:download",
        "#{id}:release:upload",
      ]
      task :release => release_tasks
    end
  end

  def define_generic_data_tasks(label,
                                id,
                                rc_dir,
                                release_dir,
                                target_files_glob)
    define_generic_data_rc_tasks(label, id, rc_dir, target_files_glob)
    define_generic_data_release_tasks(label, id, release_dir)
  end

  def define_python_tasks
    define_generic_data_tasks("Python",
                              :python,
                              "#{rc_dir}/python/#{full_version}",
                              "#{release_dir}/python/#{full_version}",
                              "{conda,wheel}-*/**/*")
  end

  def define_nuget_tasks
    define_generic_data_tasks("NuGet",
                              :nuget,
                              "#{rc_dir}/nuget/#{full_version}",
                              "#{release_dir}/nuget/#{full_version}",
                              "nuget/**/*")
  end

  def define_summary_tasks
    namespace :summary do
      desc "Show RC summary"
      task :rc do
        puts(<<-SUMMARY)
Success! The release candidate binaries are available here:
  https://bintray.com/#{bintray_repository}/debian-rc/#{full_version}
  https://bintray.com/#{bintray_repository}/ubuntu-rc/#{full_version}
  https://bintray.com/#{bintray_repository}/centos-rc/#{full_version}
  https://bintray.com/#{bintray_repository}/python-rc/#{full_version}
  https://bintray.com/#{bintray_repository}/nuget-rc/#{full_version}
        SUMMARY
      end

      desc "Show release summary"
      task :release do
        puts(<<-SUMMARY)
Success! The release binaries are available here:
  https://bintray.com/#{bintray_repository}/debian/#{version}
  https://bintray.com/#{bintray_repository}/ubuntu/#{version}
  https://bintray.com/#{bintray_repository}/centos/#{version}
  https://bintray.com/#{bintray_repository}/python/#{version}
  https://bintray.com/#{bintray_repository}/nuget/#{version}
        SUMMARY
      end
    end
  end
end
