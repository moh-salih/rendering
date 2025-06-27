#ifndef AMF_IMAGE_FETCHER_H
#define AMF_IMAGE_FETCHER_H

#include <string>
#include <thread>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace aif{
    
class ImageFetcher {
    public:
        using RawImage = std::vector<unsigned char>;
        using OneImageCallback = std::function<void(bool, RawImage)>;
        using ManyImageCallback = std::function<void(bool, std::vector<RawImage>)>;
    
        explicit ImageFetcher();
        ~ImageFetcher();
    
        // Fetchs an image from `url` asynchronously and invokes the callback when the image is ready.
        void fetchOne(const std::string& url, const OneImageCallback& callback);

        
        // Fetchs `count` images from same `url` asynchronously and invokes the callback when all `n`images are ready.
        void fetchMany(int count, const std::string& url, const ManyImageCallback& callback);
        

        void fetchManyFromUrls(const std::vector<std::string>& urls, const ManyImageCallback& callback);
    private:
        void workerLoop();
    
        struct Task {
            std::string url;
            OneImageCallback callback;
        };
    
        std::queue<Task> taskQueue;
        std::mutex queueMutex;
        std::condition_variable queueCond;
        std::thread worker;
        std::atomic<bool> running;
    };
}

#endif // AMF_IMAGE_FETCHER_H
