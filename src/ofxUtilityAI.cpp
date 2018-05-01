#include "ofxUtilityAI.h"

using namespace ofxAI;

void Selector::eval() const {
    // set the default action as the current highest-scoring version
    const QualifierActionPair* chosen = &defaultAction;
    float currentScore = defaultAction.first.threshold;

    // evaluate each of the qualifiers searching for the highest-scoring one
    for (const QualifierActionPair& current : qualifiers) {
        float newScore = current.first.score();
        if (newScore > currentScore) {
            currentScore = newScore;
            chosen = &current;
        }
    }
    // perform the action for the highest-scoring qualifier/action pair
    chosen->second();
}

float Qualifier::score() const {
    switch (type) {
    case Type::DefaultAction:
    case Type::FixedScore:
        return threshold;
    case Type::AllOrNothing:
    {
        float sum = 0;
        for (const Scorer& scorer : scorers) {
            if (!scorer.condition())
                return 0;
            sum += scorer.score();
        }
        return sum;
    }
    case Type::SumOfChildren:
    {
        float sum = 0;
        for (const Scorer& scorer : scorers) {
            if (scorer.condition())
                sum += scorer.score();
        }
        return sum;
    }
    case Type::SumWhileAboveThreshold:
    {
        float sum = 0;
        for (const Scorer& scorer : scorers) {
            if (scorer.condition()) {
                float score = scorer.score();
                if (score < threshold)
                    return sum;
                sum += score;
            }
        }
        return sum;
    }
    default:
        return 0;
    }
}
