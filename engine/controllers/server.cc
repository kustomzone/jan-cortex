#include "server.h"

#include "trantor/utils/Logger.h"
#include "utils/cortex_utils.h"
#include "utils/function_calling/common.h"

using namespace inferences;

namespace inferences {

server::server(std::shared_ptr<services::InferenceService> inference_service,
               std::shared_ptr<EngineService> engine_service)
    : inference_svc_(inference_service), engine_service_(engine_service) {
#if defined(_WIN32)
  if (bool should_use_dll_search_path = !(getenv("ENGINE_PATH"));
      should_use_dll_search_path) {
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  }
#endif
};

server::~server() {}

void server::ChatCompletion(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {
  LOG_DEBUG << "Start chat completion";
  auto json_body = req->getJsonObject();
  bool is_stream = (*json_body).get("stream", false).asBool();
  LOG_DEBUG << "request body: " << json_body->toStyledString();
  auto q = std::make_shared<services::SyncQueue>();
  auto ir = inference_svc_->HandleChatCompletion(q, json_body);
  if (ir.has_error()) {
    auto err = ir.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(std::get<1>(err));
    resp->setStatusCode(
        static_cast<HttpStatusCode>(std::get<0>(err)["status_code"].asInt()));
    callback(resp);
    return;
  }
  LOG_DEBUG << "Wait to chat completion responses";
  if (is_stream) {
    ProcessStreamRes(std::move(callback), q);
  } else {
    ProcessNonStreamRes(std::move(callback), *q);
  }
  LOG_DEBUG << "Done chat completion";
}

void server::Embedding(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback) {
  LOG_TRACE << "Start embedding";
  auto q = std::make_shared<services::SyncQueue>();
  auto ir = inference_svc_->HandleEmbedding(q, req->getJsonObject());
  if (ir.has_error()) {
    auto err = ir.error();
    auto resp = cortex_utils::CreateCortexHttpJsonResponse(std::get<1>(err));
    resp->setStatusCode(
        static_cast<HttpStatusCode>(std::get<0>(err)["status_code"].asInt()));
    callback(resp);
    return;
  }
  LOG_TRACE << "Wait to embedding";
  ProcessNonStreamRes(std::move(callback), *q);
  LOG_TRACE << "Done embedding";
}

void server::UnloadModel(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {
  auto engine = (*req->getJsonObject())["engine"].asString();
  auto model = (*req->getJsonObject())["model_id"].asString();
  CTL_INF("Unloading model: " + model + ", engine: " + engine);
  auto ir = inference_svc_->UnloadModel(engine, model);
  auto resp = cortex_utils::CreateCortexHttpJsonResponse(std::get<1>(ir));
  resp->setStatusCode(
      static_cast<HttpStatusCode>(std::get<0>(ir)["status_code"].asInt()));
  callback(resp);
}

void server::ModelStatus(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {
  auto ir = inference_svc_->GetModelStatus(req->getJsonObject());
  auto resp = cortex_utils::CreateCortexHttpJsonResponse(std::get<1>(ir));
  resp->setStatusCode(
      static_cast<HttpStatusCode>(std::get<0>(ir)["status_code"].asInt()));
  callback(resp);
}

void server::GetModels(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback) {
  LOG_TRACE << "Start to get models";
  auto ir = inference_svc_->GetModels(req->getJsonObject());
  auto resp = cortex_utils::CreateCortexHttpJsonResponse(std::get<1>(ir));
  resp->setStatusCode(
      static_cast<HttpStatusCode>(std::get<0>(ir)["status_code"].asInt()));
  callback(resp);
  LOG_TRACE << "Done get models";
}

void server::FineTuning(
    const HttpRequestPtr& req,
    std::function<void(const HttpResponsePtr&)>&& callback) {
  auto ir = inference_svc_->FineTuning(req->getJsonObject());
  auto resp = cortex_utils::CreateCortexHttpJsonResponse(std::get<1>(ir));
  resp->setStatusCode(
      static_cast<HttpStatusCode>(std::get<0>(ir)["status_code"].asInt()));
  callback(resp);
  LOG_TRACE << "Done fine-tuning";
}

void server::LoadModel(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback) {
  auto ir = inference_svc_->LoadModel(req->getJsonObject());
  auto resp = cortex_utils::CreateCortexHttpJsonResponse(std::get<1>(ir));
  resp->setStatusCode(
      static_cast<HttpStatusCode>(std::get<0>(ir)["status_code"].asInt()));
  callback(resp);
  LOG_TRACE << "Done load model";
}

void server::ProcessStreamRes(std::function<void(const HttpResponsePtr&)> cb,
                              std::shared_ptr<services::SyncQueue> q) {
  auto err_or_done = std::make_shared<std::atomic_bool>(false);
  auto chunked_content_provider =
      [q, err_or_done](char* buf, std::size_t buf_size) -> std::size_t {
    if (buf == nullptr) {
      LOG_TRACE << "Buf is null";
      return 0;
    }

    if (*err_or_done) {
      LOG_TRACE << "Done";
      return 0;
    }

    auto [status, res] = q->wait_and_pop();

    if (status["has_error"].asBool() || status["is_done"].asBool()) {
      *err_or_done = true;
    }

    auto str = res["data"].asString();
    LOG_DEBUG << "data: " << str;
    std::size_t n = std::min(str.size(), buf_size);
    memcpy(buf, str.data(), n);

    return n;
  };

  auto resp = cortex_utils::CreateCortexStreamResponse(chunked_content_provider,
                                                       "chat_completions.txt");
  cb(resp);
}

void server::ProcessNonStreamRes(std::function<void(const HttpResponsePtr&)> cb,
                                 services::SyncQueue& q) {
  auto [status, res] = q.wait_and_pop();
  function_calling_utils::PostProcessResponse(res);
  LOG_DEBUG << "response: " << res.toStyledString();
  auto resp = cortex_utils::CreateCortexHttpJsonResponse(res);
  resp->setStatusCode(
      static_cast<drogon::HttpStatusCode>(status["status_code"].asInt()));
  cb(resp);
}

}  // namespace inferences
