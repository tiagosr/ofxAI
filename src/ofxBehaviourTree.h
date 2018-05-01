#pragma once
#include <functional>
#include <string>
#include <memory>
#include <vector>
#include <stack>
#include <map>

namespace ofxAI {
    namespace BehaviourTree {
        enum class Status {
            Invalid,
            Success,
            Failure,
            Running
        };

        class Tree;

        class Blackboard {
        public:
            virtual ~Blackboard() {}
            virtual void setFact(const std::string& factName, const std::string& data) = 0;
            virtual bool getFact(const std::string& factName, std::string& factData) const = 0;
            virtual void removeFact(const std::string& factName) = 0;
            virtual bool factExists(const std::string& factName) const = 0;
            bool getFactRef(const std::string& factName, std::string& result, const Tree* tree) const;
        };


        class BaseNode {
        public:
            BaseNode(std::string const & ref) : m_ref(ref) {}
            virtual ~BaseNode() {};
            virtual Status tick(Tree* tree) = 0;

            std::string m_ref;

            using NodePtr = std::unique_ptr<BaseNode>;
            using NodeVector = std::vector<NodePtr>;
            using NodeTick = std::function<Status(Tree*, const std::vector<std::string>&)>;
            using NodeDecorate = std::function<Status(Tree*, BaseNode*, const std::vector<std::string>&)>;
        };

        /*
         * Node constructor object - stores either the leaf function, a decorator function,
         * or serves as the base for one of the built-in objects
         */
        struct Node {
            Node(const BaseNode::NodeTick& leaf) : m_leaf(leaf) {}
            Node(const BaseNode::NodeDecorate& decorator, const Node& child)
                : m_decorator(decorator)
                , m_children({ child }) {
            }
            std::string const & name() const { return m_name; }
            std::string const & ref() const { return m_ref; }
            std::vector<Node> const & children() const { return m_children; }
            std::vector<std::string> const & params() const { return m_params; }
            BaseNode::NodeTick const & leaf() const { return m_leaf; }
            BaseNode::NodeDecorate const & decorator() const { return m_decorator; }
        protected:
            Node() {}
            Node(std::string leaf, std::string const & ref) : m_name(leaf), m_ref(ref) {}
            Node(std::string const & composite, std::string const & ref, std::initializer_list<Node> children)
                : m_name(composite)
                , m_children(children) {
            }
            Node(std::string const & composite, std::string const & ref, std::initializer_list<Node> children, std::initializer_list<std::string> params)
                : m_name(composite)
                , m_children(children)
                , m_params(params) {
            }
            Node(std::string const & leaf, std::string const & ref, std::initializer_list<std::string> params)
                : m_name(leaf)
                , m_params(params) {
            }
            std::vector<Node> m_children;
            std::string m_name;
            std::string m_ref;
            std::vector<std::string> m_params;
            BaseNode::NodeTick m_leaf;
            BaseNode::NodeDecorate m_decorator;
        };


        /*
         * Sequence node: Runs children in sequence while they return Success.
         * Stops on the first one to return Failure, Running or Invalid,
         * returning that status; if every child returns Success, it returns
         * Success.
         */
        struct Sequence : public Node {
            static constexpr char *name = "Sequence";
            Sequence(std::initializer_list<Node> children)
                : Sequence("", children) {
            }
            Sequence(std::string const & ref, std::initializer_list<Node> children)
                : Node(name, ref, children) {
            }
        };


        /*
         * Selector node: Runs children in sequence while they return Failure.
         * Stops on the first one to return Success, Running or Invalid,
         * returning that status; if every child returns Failure, it returns
         * Failure.
         */
        struct Selector : public Node {
            static constexpr char *name = "Selector";
            Selector(std::initializer_list<Node> children)
                : Selector("", children) {
            }
            Selector(std::string const & ref, std::initializer_list<Node> children)
                : Node(name, ref, children) {
            }
        };


        /*
         * Parallel node: Runs every child node, collecting the amount of
         * nodes that returned Success (nSuccess) or Failure (nFailure).
         * If any children return Invalid, return Invalid.
         * If nSuccess >= threshold, return Success.
         * If nFailure > children.size()-threshold, return Failure.
         * Return Running otherwise.
         */
        struct Parallel : public Node {
            static constexpr char *name = "Parallel";
            Parallel(std::string const & ref, size_t successThreshold, size_t failureThreshold, std::initializer_list<Node> children)
                : Node(name, ref, children, {
                    std::to_string(successThreshold),
                    std::to_string(failureThreshold)
                }) {
            }
            Parallel(size_t successThreshold, size_t failureThreshold, std::initializer_list<Node> children)
                : Parallel("", successThreshold, failureThreshold, children) {
            }
            Parallel(std::string const & ref, std::initializer_list<Node> children)
                : Parallel(ref, children.size(), 1, children) {
            }
            Parallel(std::string const & ref, size_t threshold, std::initializer_list<Node> children)
                : Parallel(ref, threshold, children.size() - threshold, children) {
            }
            Parallel(std::initializer_list<Node> children)
                : Parallel("", children) {
            }
            Parallel(size_t threshold, std::initializer_list<Node> children)
                : Parallel("", threshold, children) {
            }
        };


        /*
         * First return: Runs every child node, returning Success if any
         * child returns success, and Failure if any child returns Failure,
         * else keep returning Running.
         * Returns Invalid if any child returns Invalid.
         */
        struct FirstReturn : public Node {
            static constexpr char *name = "FirstReturn";
            FirstReturn(std::initializer_list<Node> children)
                : FirstReturn("", children) {
            }
            FirstReturn(std::string const & ref, std::initializer_list<Node> children)
                : Node(name, ref, children) {
            }

        };


        /*
         * True decorator: Alters the result of the child node, turning
         * Failure into Success.
         * Other results are unaffected.
         */
        struct ReturnTrue : public Node {
            static constexpr char *name = "ReturnTrue";
            ReturnTrue(std::string const& ref, const Node& child)
                : Node(name, ref, { child }) {
            }
            ReturnTrue(const Node& child)
                : ReturnTrue("", child) {
            }
        };


        /*
         * False decorator: Alters the result of the child node, turning
         * Success into Failure.
         * Other results are unaffected.
         */
        struct ReturnFalse : public Node {
            static constexpr char *name = "ReturnFalse";
            ReturnFalse(std::string const& ref, const Node& child)
                : Node(name, ref, { child }) {
            }
            ReturnFalse(const Node& child)
                : ReturnFalse("", child) {
            }
        };


        /*
         * Negate decorator: Alters the result of the child node, exchanging
         * between Success and Failure.
         * Other results are unaffected.
         */
        struct Negate : public Node {
            static constexpr char *name = "Negate";
            Negate(std::string const& ref, const Node& child)
                : Node(name, ref, { child }) {
            }
            Negate(const Node& child)
                : Negate("", child) {
            }
        };


        /*
         * Fact exists: Returns Success if a given fact is present
         * in the current blackboard, Failure otherwise.
         */
        struct FactExists : public Node {
            static constexpr char *name = "FactExists";
            FactExists(std::string const & ref, const std::string& fact)
                : Node(name, ref, { fact }) {
            }
            FactExists(const std::string& fact)
                : FactExists("", fact) {
            }
        };


        /*
         * Remove fact: Removes a fact from the current blackboard,
         * returning Success.
         */
        struct RemoveFact : public Node {
            static constexpr char *name = "RemoveFact";
            RemoveFact(std::string const & ref, const std::string& fact)
                : Node(name, ref, { fact }) {
            }
            RemoveFact(const std::string& fact)
                : RemoveFact("", fact) {
            }
        };


        /*
         * Set fact constant: Sets a fact value in the blackboard,
         * returning Success.
         */
        struct SetFactConst : public Node {
            static constexpr char *name = "SetFactConst";
            SetFactConst(std::string const& ref, const std::string& fact, const std::string& constant)
                : Node(name, ref, { fact, constant }) {
            }
            SetFactConst(const std::string& fact, const std::string& constant)
                : SetFactConst("", fact, constant) {
            }
        };


        /*
         * Fact equals constant: Checks if a fact in the blackboard
         * has a specific value
         */
        struct FactEqualsConst : public Node {
            static constexpr char *name = "FactEqualsConst";
            FactEqualsConst(std::string const& ref, const std::string& fact, const std::string& constant)
                : Node(name, ref, { fact, constant }) {
            }
            FactEqualsConst(const std::string& fact, const std::string& constant)
                : FactEqualsConst("", fact, constant) {
            }
        };

        /*
         * Run children nodes until they return true
         */
        struct UntilTrue : public Node {
            static constexpr char *name = "UntilTrue";
            UntilTrue(std::string const& ref, std::initializer_list<Node> children)
                : Node(name, ref, children) {
            }
            UntilTrue(std::initializer_list<Node> children)
                : UntilTrue("", children) {
            }
        };
        /*
         * Run children nodes until they return false
         */
        struct UntilFalse : public Node {
            static constexpr char *name = "UntilFalse";
            UntilFalse(std::string const& ref, std::initializer_list<Node> children)
                : Node(name, ref, children) {
            }
            UntilFalse(std::initializer_list<Node> children)
                : UntilFalse("", children) {
            }
        };
        /*
         * Run children nodes unconditionally
         */
        struct AlwaysRun : public Node {
            static constexpr char *name = "AlwaysRun";
            AlwaysRun(std::string const& ref, std::initializer_list<Node> children)
                : Node(name, ref, children) {
            }
            AlwaysRun(std::initializer_list<Node> children)
                : AlwaysRun("", children) {
            }
        };

        struct Strategy : public Node {
            static constexpr char *name = "Strategy";
            Strategy(std::string const & ref, Node const & condition, Node const & action)
                : Node(name, ref, { condition, action }) {
            }
            Strategy(Node const & condition, Node const & action)
                : Strategy("", condition, action) {
            }
        };

        struct Decision : public Node {
            static constexpr char *name = "Decision";
            Decision(std::string const & ref, std::initializer_list<Decision> decisions)
                : Node(name, ref, (std::initializer_list<Node>&)decisions) {
            }
            Decision(std::initializer_list<Decision> decisions)
                : Decision("", decisions) {
            }
        };

        class NodeScope {
        public:
            NodeScope(const std::map<std::string, std::string>& values) : m_values(values) {}
            bool getScopeVar(const std::string& key, std::string& value) const;

        protected:
            std::map<std::string, std::string> m_values;
        };

        class Tree {
        public:
            using BlackboardPtr = std::shared_ptr<Blackboard>;
            using NodeScopePtr = std::unique_ptr<NodeScope>;

            Tree();
            Tree(Node const & tree);
            Tree(BlackboardPtr ptr);
            Tree(Node const & tree, BlackboardPtr ptr);


            BlackboardPtr getBlackboard() {
                return m_blackboard;
            }

            Status tick();

            bool loadTree(const Node& root);
            bool getScopedVar(const std::string& varName, std::string& output) const;
            void pushScope(NodeScopePtr scope);
            void popScope();
            static BaseNode::NodePtr createNode(Node const & node);
        protected:
            BaseNode::NodePtr m_root;
            BlackboardPtr m_blackboard;
            std::stack<NodeScopePtr> m_scopeStack;
            friend class NodeScope;
        };
    }
}

