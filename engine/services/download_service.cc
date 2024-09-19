#include <curl/curl.h>
#include <httplib.h>
#include <stdio.h>
#include <trantor/utils/Logger.h>
#include <filesystem>
#include <optional>
#include <thread>

#include "download_service.h"
#include "utils/logging_utils.h"

namespace {
size_t WriteCallback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
  size_t written = fwrite(ptr, size, nmemb, stream);
  return written;
}
}  // namespace

void DownloadService::AddDownloadTask(
    const DownloadTask& task,
    std::optional<OnDownloadTaskSuccessfully> callback) {
  CLI_LOG("Validating download items, please wait..");
  // preprocess to check if all the item are valid
  auto total_download_size{0};
  std::optional<std::string> err_msg = std::nullopt;
  for (const auto& item : task.items) {
    auto file_size = GetFileSize(item.downloadUrl);
    if (file_size.has_error()) {
      err_msg = file_size.error();
      break;
    }

    total_download_size += file_size.value();
  }

  if (err_msg.has_value()) {
    CTL_ERR(err_msg.value());
    return;
  }

  // all items are valid, start downloading
  // if any item from the task failed to download, the whole task will be
  // considered failed
  std::optional<std::string> dl_err_msg = std::nullopt;
  for (const auto& item : task.items) {
    CLI_LOG("Start downloading: " + item.localPath.filename().string());
    auto result = Download(task.id, item);
    if (result.has_error()) {
      dl_err_msg = result.error();
      break;
    }
  }
  if (dl_err_msg.has_value()) {
    CTL_ERR(dl_err_msg.value());
    return;
  }

  if (callback.has_value()) {
    callback.value()(task);
  }
}

cpp::result<uint64_t, std::string> DownloadService::GetFileSize(
    const std::string& url) const noexcept {
  CURL* curl;
  curl = curl_easy_init();

  if (!curl) {
    return cpp::fail(static_cast<std::string>("Failed to init CURL"));
  }

  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  CURLcode res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    return cpp::fail(static_cast<std::string>(
        "CURL failed: " + std::string(curl_easy_strerror(res))));
  }

  curl_off_t content_length = 0;
  res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T,
                          &content_length);
  return content_length;
}

void DownloadService::AddAsyncDownloadTask(
    const DownloadTask& task,
    std::optional<OnDownloadTaskSuccessfully> callback) {

  for (const auto& item : task.items) {
    std::thread([this, task, &callback, item]() {
      this->Download(task.id, item);
    }).detach();
  }

  // TODO: how to call the callback when all the download has finished?
}

cpp::result<void, std::string> DownloadService::Download(
    const std::string& download_id,
    const DownloadItem& download_item) noexcept {
  CTL_INF("Absolute file output: " << download_item.localPath.string());

  CURL* curl;
  FILE* file;
  CURLcode res;

  curl = curl_easy_init();
  if (!curl) {
    return cpp::fail(static_cast<std::string>("Failed to init CURL"));
  }

  file = fopen(download_item.localPath.string().c_str(), "wb");
  if (!file) {
    return cpp::fail(static_cast<std::string>(
        "Failed to open output file " + download_item.localPath.string()));
  }

  curl_easy_setopt(curl, CURLOPT_URL, download_item.downloadUrl.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    fclose(file);
    curl_easy_cleanup(curl);
    std::string err_msg{curl_easy_strerror(res)};

    return cpp::fail(
        static_cast<std::string>("Download failed! Error: " + err_msg));
  }

  fclose(file);
  curl_easy_cleanup(curl);
  return {};
}
