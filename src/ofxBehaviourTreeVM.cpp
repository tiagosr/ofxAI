#include "ofxBehaviourTreeVM.h"

namespace {
    template <size_t opcode_val, typename u_type, typename s_type>
    struct vm_opcode {
        using op_type = s_type;
        static constexpr op_type opcode = opcode_val;
        using successor = vm_opcode<opcode_val + 1, u_type, s_type>;
    };

    template <size_t opcode_val>
    using btvm_opcode = vm_opcode<opcode_val, uint16_t, int16_t>;

    const std::string empty_str;
}

namespace ofxAI {
    namespace BTVM {
        struct BehaviorTreeVMProgram {

            using bt_runner = std::function<Status(BehaviorTreeVMThread*, DictBlackboard*)>;
            using bt_decorator = std::function<Status(BehaviorTreeVMThread*, DictBlackboard*)>;

            struct ops {
                using run = btvm_opcode<0>;     // run the specified leaf node
                using run_thr = run::successor;     // run from specified stream
                using run_dec = run_thr::successor; // run a decorator
                using bra_f = run_dec::successor; // branch if current value is Failure
                using bra_t = bra_f::successor;   // branch if current value is Success
                using set_f = bra_t::successor;   // set Failure
                using set_t = set_f::successor;   // set Success
                using neg = set_t::successor;   // swap between Failure<->Success
                using chk_fact = neg::successor;     // check if fact with string (pc+1) is present in the blackboard
                using rm_fact = chk_fact::successor; // remove blackboard fact with string (pc+1)
                using dbg_break = rm_fact::successor; // break mid-tree for debugging
                using log = dbg_break::successor; // output a string along with the current state
            };

            using op_type = ops::run::op_type;
            std::vector<op_type> m_program;
            std::vector<bt_runner> m_leaves;
            std::vector<bt_decorator> m_decoratorNodes;
            std::vector<std::string> m_stringTable;

            Status eval(BehaviorTreeVM* vm, BehaviorTreeVMThread * thread, DictBlackboard * blackboard) {
                if (!thread || !blackboard)
                    return Status::Invalid;
                thread->m_current = Status::Invalid;
                op_type op = m_program[thread->m_pc];
                switch (op) {
                case ops::run::opcode:
                    thread->m_current = m_leaves[m_program[thread->m_pc + 1]](thread, blackboard);
                    if ((thread->m_current == Status::Failure) ||
                        (thread->m_current == Status::Success) ||
                        (thread->m_current == Status::Running)) {
                        thread->m_pc += 2;
                        return Status::Running;
                    }
                    else {
                        return thread->m_current;
                    }
                case ops::run_thr::opcode:
                {
                    auto& other_thread = vm->m_threads[m_program[thread->m_pc + 1]];
                    eval(vm, &other_thread, blackboard);
                    thread->m_current = other_thread.m_current;
                    if (thread->m_current != Status::Invalid) {
                        thread->m_pc += 2;
                        return Status::Running;
                    }
                    else {
                        return thread->m_current;
                    }
                }
                case ops::run_dec::opcode:
                    thread->m_current = m_leaves[m_program[thread->m_pc + 1]](thread, blackboard);
                    if ((thread->m_current == Status::Failure) ||
                        (thread->m_current == Status::Success) ||
                        (thread->m_current == Status::Running)) {
                        thread->m_pc += 2;
                        return Status::Running;
                    }
                    else {
                        return thread->m_current;
                    }

                case ops::bra_f::opcode:
                    if (thread->m_current == Status::Failure) {
                        op_type off = m_program[thread->m_pc + 1];
                        thread->m_pc += off;
                    }
                    else {
                        thread->m_pc += 2;
                    }
                    return Status::Running;
                case ops::bra_t::opcode:
                    if (thread->m_current == Status::Success) {
                        op_type off = m_program[thread->m_pc + 1];
                        thread->m_pc += off;
                    }
                    else {
                        thread->m_pc += 2;
                    }
                    return Status::Running;
                case ops::set_f::opcode:
                    thread->m_current = Status::Failure;
                    thread->m_pc++;
                    return Status::Running;
                case ops::set_t::opcode:
                    thread->m_current = Status::Success;
                    thread->m_pc++;
                    return Status::Running;
                case ops::neg::opcode:
                    thread->m_current =
                        (thread->m_current == Status::Failure ? Status::Success :
                        (thread->m_current == Status::Success ? Status::Failure :
                            thread->m_current));
                    thread->m_pc++;
                    return Status::Running;
                case ops::chk_fact::opcode:
                    if (blackboard->hasFact(m_stringTable[thread->m_pc + 1]))
                        thread->m_current = Status::Success;
                    else
                        thread->m_current = Status::Failure;
                    thread->m_pc += 2;
                    return Status::Running;
                case ops::rm_fact::opcode:
                    blackboard->removeFact(m_stringTable[thread->m_pc + 1]);
                    thread->m_current = Status::Success;
                    thread->m_pc += 2;
                    return Status::Running;
                default:
                    return Status::Invalid;
                }
            }
        };


        Status BehaviorTreeVMThread::step(BehaviorTreeVM * vm) {
            if (!vm)
                return Status::Invalid;
            return vm->m_program->eval(vm, this, &vm->blackboard);
        }

        void BehaviorTreeVMThread::reset() {
            m_pc = (off_t)m_threadStart;
            m_current = Status::Invalid;
        }

        bool DictBlackboard::hasFact(const std::string & fact) const {
            return m_board.find(fact) != m_board.end();
        }

        const std::string & DictBlackboard::getFact(const std::string & fact) const {
            auto found = m_board.find(fact);
            if (found == m_board.end())
                return empty_str;
            return found->second;
        }

        void DictBlackboard::removeFact(const std::string & fact) {
            m_board.erase(fact);
        }

        void DictBlackboard::setFact(const std::string & fact, const std::string & data) {
            m_board[fact] = data;
        }
    }
}

