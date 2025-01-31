#pragma once

#include <memory>
#include <string>

#if defined(_WIN32)
#define NOMINMAX
#endif

#include <drogon/HttpController.h>

#ifndef NDEBUG
// crash the server in debug mode, otherwise send an http 500 error
#define CPPHTTPLIB_NO_EXCEPTIONS 1
#endif

#include <cstddef>
#include <string>
#include "common/base.h"
#include "services/inference_service.h"

#ifndef SERVER_VERBOSE
#define SERVER_VERBOSE 1
#endif

using namespace drogon;

namespace inferences {

class server : public drogon::HttpController<server, false>,
               public BaseModel,
               public BaseChatCompletion,
               public BaseEmbedding {
 public:
  server(std::shared_ptr<services::InferenceService> inference_service,
         std::shared_ptr<EngineService> engine_service);
  ~server();
  METHOD_LIST_BEGIN
  // list path definitions here;
  METHOD_ADD(server::ChatCompletion, "chat_completion", Options, Post);
  METHOD_ADD(server::Embedding, "embedding", Options, Post);
  METHOD_ADD(server::LoadModel, "loadmodel", Options, Post);
  METHOD_ADD(server::UnloadModel, "unloadmodel", Options, Post);
  METHOD_ADD(server::ModelStatus, "modelstatus", Options, Post);
  METHOD_ADD(server::GetModels, "models", Get);

  // cortex.python API
  METHOD_ADD(server::FineTuning, "finetuning", Options, Post);

  // Openai compatible path
  ADD_METHOD_TO(server::ChatCompletion, "/v1/chat/completions", Options, Post);
  ADD_METHOD_TO(server::FineTuning, "/v1/fine_tuning/job", Options, Post);
  ADD_METHOD_TO(server::Embedding, "/v1/embeddings", Options, Post);

  METHOD_LIST_END
  void ChatCompletion(
      const HttpRequestPtr& req,
      std::function<void(const HttpResponsePtr&)>&& callback) override;
  void Embedding(
      const HttpRequestPtr& req,
      std::function<void(const HttpResponsePtr&)>&& callback) override;
  void LoadModel(
      const HttpRequestPtr& req,
      std::function<void(const HttpResponsePtr&)>&& callback) override;
  void UnloadModel(
      const HttpRequestPtr& req,
      std::function<void(const HttpResponsePtr&)>&& callback) override;
  void ModelStatus(
      const HttpRequestPtr& req,
      std::function<void(const HttpResponsePtr&)>&& callback) override;
  void GetModels(
      const HttpRequestPtr& req,
      std::function<void(const HttpResponsePtr&)>&& callback) override;
  void FineTuning(
      const HttpRequestPtr& req,
      std::function<void(const HttpResponsePtr&)>&& callback) override;

 private:
  void ProcessStreamRes(std::function<void(const HttpResponsePtr&)> cb,
                        std::shared_ptr<services::SyncQueue> q);
  void ProcessNonStreamRes(std::function<void(const HttpResponsePtr&)> cb,
                           services::SyncQueue& q);

 private:
  std::shared_ptr<services::InferenceService> inference_svc_;
  std::shared_ptr<EngineService> engine_service_;
};
};  // namespace inferences
