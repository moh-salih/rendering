#include "ImageFetcher.h"
#include <cpr/cpr.h>
#include <iostream>

namespace aif{ 

    ImageFetcher::ImageFetcher() : running(true), worker(&ImageFetcher::workerLoop, this) {}

    ImageFetcher::~ImageFetcher() {
        running = false;
        queueCond.notify_all();
        if (worker.joinable()) {
            worker.join();
        }
    }

    void ImageFetcher::fetchOne(const std::string& url, const OneImageCallback& callback) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            taskQueue.push(Task{url, callback});
        }
        queueCond.notify_one();
    }
    
    void ImageFetcher::fetchMany(int count, const std::string& url, const ManyImageCallback& callback){
        struct Batch {
            int remaining;
            std::string url;
            std::vector<RawImage> results;
            ManyImageCallback callback;
            std::mutex mutex;
        };

        auto batch = std::make_shared<Batch>();
        batch->remaining = count;
        batch->url = url;
        batch->callback = callback;

        auto oneDone = [batch](bool success, const RawImage& data) {
            {
                std::lock_guard<std::mutex> lock(batch->mutex);
                if (success) {
                    batch->results.push_back(data);
                }
                batch->remaining--;
            }

            if (batch->remaining == 0) {
                bool any = !batch->results.empty();
                batch->callback(any, batch->results);
            }
        };

        for (int i = 0; i < count; ++i) {
            fetchOne(url, oneDone);
        }
    }
    
    void ImageFetcher::fetchManyFromUrls(const std::vector<std::string>& urls, const ManyImageCallback& callback){
        struct Batch{
            int remaining;
            std::vector<RawImage> results;
            ManyImageCallback callback;
            std::mutex mutex;
        };

        auto batch = std::make_shared<Batch>();
        batch->remaining = urls.size();
        batch->callback = callback;

        auto oneDone = [batch](bool success, const RawImage& data){
            std::lock_guard<std::mutex> lock(batch->mutex);
            if(success){
                batch->results.push_back(data);
            }
            batch->remaining--;

            if(batch->remaining == 0){
                bool anySuccess = !batch->results.empty();
                batch->callback(anySuccess, std::move(batch->results));
            }
        };
        for(const auto& url: urls){
            fetchOne(url, oneDone);
        }
    }

    void ImageFetcher::workerLoop() {
        while (running) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                queueCond.wait(lock, [this]() { return !taskQueue.empty() || !running; });

                if (!running && taskQueue.empty()) return;

                task = std::move(taskQueue.front());
                taskQueue.pop();
            }

            auto response = cpr::Get(cpr::Url{task.url});
            if (response.status_code == 200) {
                task.callback(true, RawImage(response.text.cbegin(), response.text.cend()));
            } else {
                std::string errorMessage =  "HTTP error code: " + std::to_string(response.status_code);
                task.callback(false, RawImage(errorMessage.cbegin(), errorMessage.cend()));
            }
        }
    }

}