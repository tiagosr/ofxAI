#include "ofxBehaviourTree.h"

namespace {
    using Blackboard = ofxAI::BehaviourTree::Blackboard;
    using BaseNode = ofxAI::BehaviourTree::BaseNode;
    using Tree = ofxAI::BehaviourTree::Tree;
    using Status = ofxAI::BehaviourTree::Status;
    using NodeScope = ofxAI::BehaviourTree::NodeScope;
    using NodePtr = BaseNode::NodePtr;

    
    template <typename T, typename U>
    inline std::unique_ptr<T> static_unique_ptr_cast(std::unique_ptr<U> &&ptr) {
        U * const base_ptr = ptr.release();
        return std::unique_ptr<T>(static_cast<T*>(base_ptr));
    }


    // default blackboard implementation - a plain dictionary-backed blackboard
    class DictBlackboard : public Blackboard {
    public:
        virtual void setFact(const std::string& factName, const std::string& data) override {
            m_dict[factName] = data;
        }
        virtual bool getFact(const std::string& fact, std::string& factData) const override {
            auto found = m_dict.find(fact);
            if (found == m_dict.end())
                return false;
            factData = found->second;
            return true;
        }
        virtual void removeFact(const std::string& factName) override {
            m_dict.erase(factName);
        }
        virtual bool factExists(const std::string& factName) const override {
            return m_dict.find(factName) != m_dict.end();
        }

    protected:
        std::map<std::string, std::string> m_dict;
    };

    

    // generic leaf node, runs a function object on tick
    class LeafNode : public BaseNode {
    public:
        LeafNode(std::string const & ref, NodeTick tick, const std::vector<std::string>& params)
            : BaseNode(ref), m_tick(tick), m_params(params) {}
        virtual Status tick(Tree* tree) override {
            if (!m_tick)
                return Status::Invalid;
            return m_tick(tree, m_params);
        }
    protected:
        NodeTick m_tick;
        std::vector<std::string> m_params;
    };

    // generic decorator node, runs a filter on the return value for 
    class DecoratorNode : public BaseNode {
    public:
        DecoratorNode(std::string const & ref, NodeDecorate tick, const std::vector<std::string>& params, NodePtr child)
            : BaseNode(ref), m_tick(tick), m_params(params), m_child(std::move(child)) {}
        virtual Status tick(Tree* tree) override {
            return m_tick(tree, m_child.get(), m_params);
        }
    protected:
        NodeDecorate m_tick;
        NodePtr m_child;
        std::vector<std::string> m_params;
    };

    class SelectorNode: public BaseNode {
    public:
        SelectorNode(std::string const & ref, NodeVector& children)
            : BaseNode(ref), m_children(std::move(children)) {}
        virtual Status tick(Tree* tree) override {
            if (m_children.empty())
                return Status::Invalid;

            for (auto& child : m_children) {
                if (!child)
                    return Status::Invalid;
                auto status = child->tick(tree);
                if (status != Status::Failure)
                    return status;
            }
            return Status::Success;
        }
    protected:
        NodeVector m_children;
    };

    class SequenceNode : public BaseNode {
    public:
        SequenceNode(std::string const & ref, NodeVector& children)
            : BaseNode(ref), m_children(std::move(children)) {}
        virtual Status tick(Tree* tree) override {
            if (m_children.empty())
                return Status::Invalid;

            for (auto& child : m_children) {
                if (!child)
                    return Status::Invalid;
                auto status = child->tick(tree);
                if (status != Status::Success)
                    return status;
            }
            return Status::Success;
        }
    protected:
        NodeVector m_children;
    };

    class ParallelNode : public BaseNode {
    public:
        ParallelNode(std::string const & ref, size_t threshold, NodeVector& children)
            : BaseNode(ref)
            , m_children(std::move(children))
            , m_threshold(threshold)
        {}
        ParallelNode(std::string const & ref, NodeVector& children)
            : ParallelNode(ref, children.size() - 1, children) {
        }
        virtual Status tick(Tree* tree) override {
            
            return Status();
        }
    protected:
        NodeVector m_children;
        size_t m_threshold;
    };

    template <const Status status>
    class SimpleDecoratorNode : public BaseNode {
    public:
        SimpleDecoratorNode(std::string const & ref, NodePtr& child)
            : BaseNode(ref), m_child(std::move(child)) {}
        virtual Status tick(Tree* tree) override {
            if (!m_child) return Status::Invalid;
            auto childStatus = m_child->tick(tree);
            if ((childStatus == Status::Success) ||
                (childStatus == Status::Failure))
                return status;
            return childStatus;
        }
    protected:
        NodePtr m_child;
    };

    class FalseDecoratorNode : public SimpleDecoratorNode<Status::Failure> {
    public:
        FalseDecoratorNode(std::string const & ref, NodePtr& child)
            : SimpleDecoratorNode(ref, child) {
        }
    };
    class TrueDecoratorNode : public SimpleDecoratorNode<Status::Success> {
    public:
        TrueDecoratorNode(std::string const & ref, NodePtr& child)
            : SimpleDecoratorNode(ref, child) {
        }
    };

    class NegateDecoratorNode : public BaseNode {
    public:
        NegateDecoratorNode(std::string const & ref, NodePtr& child)
            : BaseNode(ref), m_child(std::move(child)) {}
        virtual Status tick(Tree* tree) override {
            if (!m_child) return Status::Invalid;
            auto childStatus = m_child->tick(tree);
            if (childStatus == Status::Success)
                return Status::Failure;
            if (childStatus == Status::Failure)
                return Status::Success;
            return childStatus;
        }
    protected:
        NodePtr m_child;
    };

    class RepeatDecoratorNode : public BaseNode {
    public:
        RepeatDecoratorNode(std::string const & ref, size_t loopCount, NodePtr& child)
            : BaseNode(ref), m_loopCount(loopCount), m_child(std::move(child)) {}
        virtual Status tick(Tree* tree) override {
            Status status = Status::Invalid;
            if (!m_child)
                return status;
            for (size_t i = m_loopCount; i > 0; i--) {
                status = m_child->tick(tree);
                if ((status == Status::Running) ||
                    (status == Status::Invalid))
                    return status;
            }
            return status;
        }
    protected:
        NodePtr m_child;
        size_t m_loopCount;
    };

    class RepeatWhileSuccessfulNode : public SequenceNode {
    public:
        RepeatWhileSuccessfulNode(std::string const & ref, NodeVector& children)
            : SequenceNode(ref, children) {}
        virtual Status tick(Tree* tree) override {
            if (m_children.empty())
                return Status::Invalid;
            for (auto& child : m_children) {
                Status status = child->tick(tree);
                if (status != Status::Success)
                    return status;
            }
            return Status::Running;
        }
    };

    class RepeatWhileFailureNode : public SequenceNode {
    public:
        RepeatWhileFailureNode(std::string const & ref, NodeVector& children)
            : SequenceNode(ref, children) {}
        virtual Status tick(Tree* tree) override {
            if (m_children.empty())
                return Status::Invalid;
            for (auto& child : m_children) {
                Status status = child->tick(tree);
                if (status != Status::Failure)
                    return status;
            }
            return Status::Running;
        }
    };

    class FactExistsNode : public BaseNode {
    public:
        FactExistsNode(std::string const & ref, const std::string& factName)
            : BaseNode(ref), m_factName(factName) {}
        virtual Status tick(Tree* tree) override {
            return tree->getBlackboard()->factExists(m_factName)
                ? Status::Success
                : Status::Failure;
        }
    protected:
        std::string m_factName;
    };

    class RemoveFactNode : public BaseNode {
    public:
        RemoveFactNode(std::string const & ref, const std::string& factName)
            : BaseNode(ref), m_factName(factName) {}
        virtual Status tick(Tree* tree) override {
            tree->getBlackboard()->removeFact(m_factName);
            return Status::Success;
        }
    protected:
        std::string m_factName;
    };

    class SetFactConstNode : public BaseNode {
    public:
        SetFactConstNode(std::string const & ref, const std::string& factName, const std::string& factData)
            : BaseNode(ref), m_factName(factName), m_factData(factData) {}
        virtual Status tick(Tree* tree) override {
            std::string factName;
            std::string factData;
            auto blackboard = tree->getBlackboard();
            if (!blackboard->getFactRef(m_factName, factName, tree))
                return Status::Invalid;
            if (!blackboard->getFactRef(m_factData, factData, tree))
                return Status::Invalid;
            blackboard->setFact(factName, factData);
            return Status::Success;
        }
    protected:
        std::string m_factName;
        std::string m_factData;
    };

    class FactEqualsConstantNode : public BaseNode {
    public:
        FactEqualsConstantNode(std::string const & ref, const std::string& factName, const std::string& factData)
            : BaseNode(ref), m_factName(factName), m_factData(factData) {}
        virtual Status tick(Tree* tree) override {
            std::string factName;
            std::string factData;
            auto blackboard = tree->getBlackboard();
            if (!blackboard->getFactRef(m_factName, factName, tree))
                return Status::Invalid;
            if (!blackboard->getFactRef(m_factData, factData, tree))
                return Status::Invalid;
            std::string fact;
            if (!blackboard->getFact(factName, fact))
                return Status::Invalid;
            return fact == factData
                ? Status::Success
                : Status::Failure;
        }
    protected:
        std::string m_factName;
        std::string m_factData;
    };

    class ScopeNode : public BaseNode {
    public:
        virtual Status tick(Tree* tree) override {
            std::map<std::string, std::string> params;
            auto blackboard = tree->getBlackboard();
            for (auto& item : m_params) {
                std::string temp;
                if (!blackboard->getFactRef(item.second, temp, tree)) {
                    return Status::Invalid;
                }
                params[item.first] = temp;
            }
            tree->pushScope(std::make_unique<NodeScope>(params));
            auto result = m_child->tick(tree);
            tree->popScope();
            return result;
        }
    protected:
        std::map<std::string, std::string> m_params;
        std::shared_ptr<BaseNode> m_child;
    };

    class StrategyNode : public BaseNode {
    public:
        virtual Status tick(Tree* tree) override {
            Status result = Status::Invalid;
            return result;
        }
        NodePtr m_condition;
        NodePtr m_action;
    };

    class DecisionNode : public BaseNode {
    public:
        using StrategyNodePtr = std::unique_ptr<StrategyNode>;
        using StrategyNodeVector = std::vector<StrategyNodePtr>;
        DecisionNode(std::string const & ref, StrategyNodeVector& strategies)
            : BaseNode(ref), m_strategies(std::move(strategies)) {}
        virtual Status tick(Tree* tree) override {
            Status result = Status::Invalid;
            if (m_current) {
                result = m_current->m_action->tick(tree);
                if (result != Status::Running) {
                    m_current = nullptr;
                }
                return result;
            }
            else {
                for (auto& strategy : m_strategies) {
                    Status condition = strategy->m_condition->tick(tree);
                    if (condition == Status::Success) {
                        result = strategy->m_action->tick(tree);
                        if (result == Status::Running) {
                            m_current = strategy.get();
                        }
                        return result;
                    }
                    else if (condition != Status::Failure) {
                        return condition;
                    }
                }
            }
            return result;
        }
    protected:
        StrategyNodeVector m_strategies;
        StrategyNode* m_current;
    };

    using namespace ofxAI::BehaviourTree;
    using NodePtr = BaseNode::NodePtr;

    std::map<std::string, std::function<NodePtr(Node const&)>> nodeFactory = {
        {Selector::name, [](Node const& node)->NodePtr {
            BaseNode::NodeVector children;
            for (auto inner : node.children()) {
                children.push_back(Tree::createNode(inner));
            }
            return std::make_unique<SelectorNode>(node.ref(), children);
        }},
        {Sequence::name, [](Node const& node)->NodePtr {
            BaseNode::NodeVector children;
            for (auto inner : node.children()) {
                children.push_back(Tree::createNode(inner));
            }
            return std::make_unique<SequenceNode>(node.ref(), children);
        }},
        {Parallel::name, [](Node const& node)->NodePtr {
            BaseNode::NodeVector children;
            for (auto inner : node.children()) {
                children.push_back(Tree::createNode(inner));
            }
            if (node.params().empty())
                return std::make_unique<ParallelNode>(node.ref(), children);
            else
                return std::make_unique<ParallelNode>(node.ref(), std::atoi(node.params()[0].c_str()), children);
        }},
        {UntilFalse::name, [](Node const& node)->NodePtr {
            BaseNode::NodeVector children;
            for (auto inner : node.children()) {
                children.push_back(Tree::createNode(inner));
            }
            return std::make_unique<RepeatWhileSuccessfulNode>(node.ref(), children);
        }},
        {UntilTrue::name, [](Node const& node)->NodePtr {
            BaseNode::NodeVector children;
            for (auto inner : node.children()) {
                children.push_back(Tree::createNode(inner));
            }
            return std::make_unique<RepeatWhileFailureNode>(node.ref(), children);
        }},
        {ReturnTrue::name, [](Node const& node)->NodePtr {
            return std::make_unique<TrueDecoratorNode>(node.ref(), Tree::createNode(node.children()[0]));
        }},
        {ReturnFalse::name, [](Node const& node)->NodePtr {
            return std::make_unique<FalseDecoratorNode>(node.ref(), Tree::createNode(node.children()[0]));
        }},
        {Negate::name, [](Node const& node)->NodePtr {
            return std::make_unique<NegateDecoratorNode>(node.ref(), Tree::createNode(node.children()[0]));
        }},
        {FactExists::name, [](Node const& node)->NodePtr {
            return std::make_unique<FactExistsNode>(node.ref(), node.params()[0]);
        }},
        {RemoveFact::name, [](Node const& node)->NodePtr {
            return std::make_unique<RemoveFactNode>(node.ref(), node.params()[0]);
        }},
        {SetFactConst::name, [](Node const& node)->NodePtr {
            return std::make_unique<SetFactConstNode>(node.ref(), node.params()[0], node.params()[1]);
        }},
        {FactEqualsConst::name, [](Node const& node)->NodePtr {
            return std::make_unique<FactEqualsConstantNode>(node.ref(), node.params()[0], node.params()[1]);
        }},
    };
}



inline ofxAI::BehaviourTree::Tree::Tree()
    : Tree(std::make_shared<DictBlackboard>())
{}

ofxAI::BehaviourTree::Tree::Tree(const Node & tree)
    : Tree()
{
    loadTree(tree);
}

ofxAI::BehaviourTree::Tree::Tree(BlackboardPtr ptr)
    : m_blackboard(ptr) {}

ofxAI::BehaviourTree::Tree::Tree(const Node & tree, BlackboardPtr ptr)
    : Tree(ptr) {
    loadTree(tree);
}

ofxAI::BehaviourTree::Status ofxAI::BehaviourTree::Tree::tick() {
    if (!m_root)
        return Status::Invalid;
    return m_root->tick(this);
}


bool ofxAI::BehaviourTree::Tree::loadTree(const Node & root) {
    m_root = createNode(root);
    return !!m_root;
}

bool ofxAI::BehaviourTree::Tree::getScopedVar(const std::string & varName, std::string & output) const {
    if (m_scopeStack.empty())
        return false;
    return m_scopeStack.top()->getScopeVar(varName, output);
}

void ofxAI::BehaviourTree::Tree::pushScope(NodeScopePtr scope) {
    m_scopeStack.push(std::move(scope));
}

void ofxAI::BehaviourTree::Tree::popScope() {
    m_scopeStack.pop();
}

ofxAI::BehaviourTree::BaseNode::NodePtr ofxAI::BehaviourTree::Tree::createNode(const Node & node) {
    if (node.leaf()) {
        return std::make_unique<LeafNode>(node.ref(), node.leaf(), node.params());
    }
    if (node.decorator()) {
        return std::make_unique<DecoratorNode>(
            node.ref(),
            node.decorator(),
            node.params(),
            createNode(node.children()[0]));
    }
    auto found = nodeFactory.find(node.name());
    if (found != nodeFactory.end()) {
        return found->second(node);
    }

    if (node.name() == FactEqualsConst::name) {
        return std::make_unique<FactEqualsConstantNode>(node.ref(), node.params()[0], node.params()[1]);
    }
    if (node.name() == Decision::name) {
        DecisionNode::StrategyNodeVector children;
        for (auto inner : node.children()) {
            children.push_back(static_unique_ptr_cast<StrategyNode>(createNode(inner)));
        }
        return std::make_unique<DecisionNode>(node.ref(), children);
    }
    return BaseNode::NodePtr();
}

bool ofxAI::BehaviourTree::Blackboard::getFactRef(
    const std::string & factName,
    std::string& result,
    const Tree* tree) const {
    if (factName.empty())
        return false; // won't be able to do much without a fact name
    result = factName;
    // if we want to get a scope variable
    if (factName.at(0) == '#') {
        // get the scope var name by removing the #
        std::string temp = std::string(++factName.begin(), factName.end());
        // try and get the scoped var from the tree
        if (!tree || !tree->getScopedVar(temp, temp))
            return false;
        return getFactRef(temp, result, tree);
    }
    // if we want to get an indirect variable
    if (factName.at(0) == '@') {
        // get the key by removing the @
        std::string temp = std::string(++factName.begin(), factName.end());
        // deal with multiple indirection
        if (!getFactRef(temp, temp, tree))
            return false; // indirection didn't return a fact name
        // retrieve the final fact
        if (!getFact(temp, result))
            return false; // fact was actually not there
    }
    return true; // fact was there, or just wasn't a reference after all
}

inline bool ofxAI::BehaviourTree::NodeScope::getScopeVar(const std::string & key, std::string & value) const {
    auto found = m_values.find(key);
    if (found == m_values.end())
        return false;
    value = found->second;
    return true;
}
