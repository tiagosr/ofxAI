#pragma once

#include "ofTypes.h"
#include "ofConstants.h"
#include "ofParameter.h"
#include "ofParameterGroup.h"
#include <functional>

namespace ofxAI {

    struct Scorer {
        using Condition = function<bool()>;
        using Score = function<float()>;
        Condition condition;
        Score score;

        // returns a condition that negates the current condition
        static Condition negate(Condition cond) {
            return [cond]() { return !cond(); };
        }
        Scorer(const ofParameter<float>& score, const Condition& condition)
            : score([&]() { return score; })
            , condition(condition) { }
        Scorer(const ofParameter<float>& score, const ofParameter<bool>& condition)
            : score([&]() { return score; })
            , condition([&]() { return condition; }) {}
        Scorer(float score, const Condition& condition)
            : score([=]() { return score; })
            , condition(condition) { }
        Scorer(float score, const ofParameter<bool>& condition)
            : score([=]() { return score; })
            , condition([&]() { return condition; }) { }

    };

    class Selector;

    struct Qualifier {
        enum class Type {
            DefaultAction,
            FixedScore,
            AllOrNothing,
            SumOfChildren,
            SumWhileAboveThreshold
        };
        string name;
        
        vector<Scorer> scorers;
        Type type;
        float threshold;
        float score() const;

        // Creates a qualifier that sets a fixed score
        static Qualifier FixedScore(const string& name, float threshold) {
            return Qualifier(Type::FixedScore, name, threshold, {});
        }

        // Creates a qualifier that only sets a score while every single scorer sets a score above the threshold
        static Qualifier AllOrNothing(const string& name, float threshold, initializer_list<Scorer> scorers) {
            return Qualifier(Type::AllOrNothing, name, threshold, scorers);
        }

        // Creates a qualifier that sums up all successful scorers
        static Qualifier SumOfChildren(const string& name, initializer_list<Scorer> scorers) {
            return Qualifier(Type::SumOfChildren, name, 0, scorers);
        }

        // Sums the scorers up while the individual results are above the threshold
        static Qualifier SumWhileAboveThreshold(const string& name, float threshold, initializer_list<Scorer> scorers) {
            return Qualifier(Type::SumWhileAboveThreshold, name, threshold, scorers);
        }
    protected:
        static Qualifier DefaultAction() {
            return Qualifier(Type::DefaultAction, "Default Action", 0, {});
        }
        Qualifier(Type type, const string& name, float threshold, initializer_list<Scorer> scorers)
            : type(type), threshold(threshold), name(name), scorers(scorers) { }
        friend class Selector;
    };

    using Action = std::function<void()>;
    class Selector {
    public:
        using QualifierActionPair = pair<Qualifier, Action>;
        Selector(Action action = []() {})
            : defaultAction(make_pair(Qualifier::DefaultAction(), action)) { }
        Selector(initializer_list<QualifierActionPair> qualifiers, Action action = []() {})
            : defaultAction(make_pair(Qualifier::DefaultAction(), action))
            , qualifiers(qualifiers) { }
        vector<QualifierActionPair> qualifiers;
        QualifierActionPair defaultAction;
        void eval() const;
        Action select() const { return [this]() { this->eval(); }; }
    };

}