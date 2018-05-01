#pragma once
#include <functional>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <memory>

namespace ofxAI {
    namespace BTVM {

        enum class Status {
            Invalid,
            Success,
            Failure,
            Running,
            Suspended
        };

        class DictBlackboard {
        public:
            bool hasFact(const std::string& fact) const;
            const std::string& getFact(const std::string& fact) const;
            void removeFact(const std::string& fact);
            void setFact(const std::string& fact, const std::string& data);
        protected:
            std::map<std::string, std::string> m_board;
        };

        class BehaviorTreeVM;

        struct BehaviorTreeVMThread {
            Status step(BehaviorTreeVM* vm);
            void reset();
            off_t m_pc;
            size_t m_threadStart;
            Status m_current;
        };


        struct BehaviorTreeVMProgram;

        class BehaviorTreeVM {
        public:

            DictBlackboard blackboard;
        protected:
            std::shared_ptr<BehaviorTreeVMProgram> m_program;
            std::vector<BehaviorTreeVMThread> m_threads;
            friend struct BehaviorTreeVMProgram;
            friend struct BehaviorTreeVMThread;
        };

    }
}
