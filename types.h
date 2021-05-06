#pragma once
#include <thread>
namespace translateLocally {

    class marianSettings {
    private:
        size_t cpu_cores_;
        size_t workspace_;
    public:
        marianSettings() : cpu_cores_(std::thread::hardware_concurrency()), workspace_(128) {}
        marianSettings(size_t cores, size_t memory) : cpu_cores_(cores), workspace_(memory) {}
        void setCores(size_t cores) {
            cpu_cores_ = cores;
        }
        void setWorkspace(size_t memory) {
            workspace_ = memory;
        }

        size_t getCores() const {
            return cpu_cores_;
        }
        size_t getWorkspace() const {
            return workspace_;
        }
    };

} // namespace translateLocally
