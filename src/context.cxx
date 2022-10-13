#include <string>
#include <vector>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <optional>
#include <stdexcept>
#include <algorithm>
#include <math.h>
#include <iterator>
#include <stack>
#include <list>
#include <limits>
#include <math.h>

#include "context.h"
#include "error.h"
#include "parser.h"
#include "writer.h"
#include "diagram.h"
#include "fileNode.h"
#include "DEFAULT_VALUES.h"

#define LPP_FILENAME_OFFSET 3

namespace lcc {

std::unordered_map<REACTION_TYPE, std::string> reactionTypeToAcronym = {
        {REACTION_TYPE::SU,    "SU"},
        {REACTION_TYPE::SAI,   "SAI"},
        {REACTION_TYPE::SAA,   "SAA"},
        {REACTION_TYPE::ESU,   "ESU"},
        {REACTION_TYPE::MMU,   "MMU"},
        {REACTION_TYPE::RB,    "RB"},
        {REACTION_TYPE::CBSU,  "CBSU"},
        {REACTION_TYPE::CBESU, "CBESU"},
        {REACTION_TYPE::CBMMU, "CBMMU"},
};

namespace {
    std::vector<REACTION_TYPE> validReactionTypes = {REACTION_TYPE::SU, REACTION_TYPE::SAI, REACTION_TYPE::SAA, REACTION_TYPE::ESU, REACTION_TYPE::MMU, REACTION_TYPE::RB, REACTION_TYPE::CBSU, REACTION_TYPE::CBESU, REACTION_TYPE::CBMMU};
    std::unordered_set<PARAM> validReactionParameters = {PARAM::K, PARAM::KREV, PARAM::KCAT, PARAM::KM, PARAM::Ki, PARAM::Ka, PARAM::n_param};
    std::unordered_map<REACTION_TYPE, std::vector<PARAM>> requiredParams = {
            {REACTION_TYPE::SU,  {PARAM::K,    PARAM::KREV}},
            {REACTION_TYPE::SAI, {PARAM::Ki,   PARAM::n_param}},
            {REACTION_TYPE::SAA, {PARAM::Ka,   PARAM::n_param}},
            {REACTION_TYPE::ESU, {PARAM::K,    PARAM::KREV}},
            {REACTION_TYPE::MMU, {PARAM::KCAT, PARAM::KM}}
    };
}

/* FIXEDCOUNTHANDLER */

// FixedCountHandler is an implementation-only class to handle molecule assignments.
// See wiki/FixedCountHandler Overview/ for an overview of what public member functions and private member variables are meant to do.
class FixedCountHandler {
    public:
        // newMolecule cannot be NULL
        FixedCountHandler(Molecule* newMolecule);
        ~FixedCountHandler();

        /* This function is used to get the FixedCountHandler of a molecule, which is an otherwise inaccessible
         * private member variable (possible as FixedCountHandler is declared as a friend class of Molecule in
         * Molecule's declaration). This is done so that only the implementation can access FixedCountHandlers, rather
         * than exposing them to everyone via a getter in the Molecule interface.
         * */
        static FixedCountHandler* getFixedCountHandler(Molecule* molecule);

        std::optional<double> getBaseline();
        void setBaseline(double newBaseline);

        const std::map<double, double>& getChangePoints();
        void addChangePoint(double time, double value);

        const std::map<double, std::optional<double>>& getIntervalPoints();
        void addInterval(double value, double startTime, double endTime);
    private:
        Molecule* molecule;

        std::optional<double> baseline;

        std::map<double, double> changePoints;

        // Tuples are <start, end, value>. Order is important: has intervals in the order they were added.
        std::vector<std::tuple<double, double, double>> intervals;
        // Tuples are <start, end, possible value>.
        std::list<std::tuple<double, double, std::optional<double>>> processedIntervals;
        // Points are <time, value to change to>. See wiki/Compiler Context/Interface Notes/Constant, Changed and Fixed Molecules (Interface)/ for more info.
        std::map<double, std::optional<double>> intervalPoints;
        bool haveBeenProcessed = false;

        void processIntervals();
        void removeExtraIntervals();
        void mergeIntervals();
        void convertIntervals();
};

FixedCountHandler::FixedCountHandler(Molecule* newMolecule) :
        molecule(newMolecule),
        baseline(std::nullopt),
        intervals(),
        changePoints() {
}

FixedCountHandler::~FixedCountHandler() = default;

FixedCountHandler* FixedCountHandler::getFixedCountHandler(Molecule* molecule) {
    return molecule->fixedCountHandler;
}

std::optional<double> FixedCountHandler::getBaseline() {
    return baseline;
}

void FixedCountHandler::setBaseline(double newBaseline) {
    if (baseline.has_value()) {
        std::cout << "Warning: assignment to molecule " << molecule->getName() << " of fixed count " << newBaseline << " shadows previous assignment of count " << baseline.value() << "." << std::endl;
    }
    baseline = std::optional<double>(newBaseline);
    molecule->setInitialCount(newBaseline);
    molecule->getCompartment()->hasConstantMolecules = true;
}

const std::map<double, double>& FixedCountHandler::getChangePoints() {
    return changePoints;
}

void FixedCountHandler::addChangePoint(double time, double value) {
    if (time < 0) {
        error("Assignment to molecule " + molecule->getName() + " of count " + std::to_string(value) + " at time " + std::to_string(time) + " has invalid negative time.");
    }
    if (changePoints.count(time) != 0) {
        std::cout << "Warning: assignment to molecule " << molecule->getName() << " of count " << value << " at time " << time << " shadows previous assignment of count " << changePoints[time] << "." << std::endl;
    }
    changePoints[time] = value;
    molecule->getCompartment()->hasChangedMolecules = true;
}

// See wiki/Compiler Context/Implementation Notes/getIntervalPoints() Overview/
const std::map<double, std::optional<double>>& FixedCountHandler::getIntervalPoints() {
    if (!haveBeenProcessed) {
        processIntervals();
        removeExtraIntervals();
        mergeIntervals();
        convertIntervals();
        haveBeenProcessed = true;
    }
    return intervalPoints;
}

void FixedCountHandler::addInterval(double value, double startTime, double endTime) {
    if (startTime < 0) {
        error("Assignment to molecule " + molecule->getName() + " of count " + std::to_string(value) + " at times (start, end) = (" + std::to_string(startTime) + ", " + std::to_string(endTime) + ") has invalid negative start time.");
    } else if (endTime < 0) {
        error("Assignment to molecule " + molecule->getName() + " of count " + std::to_string(value) + " at times (start, end) = (" + std::to_string(startTime) + ", " + std::to_string(endTime) + ") has invalid negative end time.");
    } else if (endTime < startTime) {
        error("Assignment to molecule " + molecule->getName() + " of count " + std::to_string(value) + " at times (start, end) = (" + std::to_string(startTime) + ", " + std::to_string(endTime) + ") has end time less than start time.");
    }

    if (startTime == 0 && isinf(endTime)) {
        this->setBaseline(value);
    } else {
        std::tuple<double, double, double> interval(startTime, endTime, value);
        intervals.push_back(interval);
        haveBeenProcessed = false;
        molecule->getCompartment()->hasFixedMolecules = true;
    }
}

// See wiki/Compiler Context/Implementation Notes/processIntervals()/
void FixedCountHandler::processIntervals() {
    constexpr int TIME_INDEX = 0;
    constexpr int VALUE_INDEX = 1;
    constexpr int INDEX_INDEX = 2;
    constexpr int IS_END_INDEX = 3;

    // Tuples are of the form (time, value, isEnd, index)
    // if isEnd = false, time refers to time that this interval starts. Otherwise, time that it ends.
    // index is for sorting later so that we preserve declaration order

    // processedIntervals.CODEPOINT 1
    std::vector<std::tuple<double, double, int, bool>> startAndEndTuples;
    {
        int index = 0;
        for (auto& [startTime, endTime, value] : intervals) {
            std::tuple<double, double, int, bool> startTuple(startTime, value, index, false);
            startAndEndTuples.push_back(startTuple);
            if (!isinf(endTime)) {
                std::tuple<double, double, int, bool> endTuple(endTime, value, index, true);
                startAndEndTuples.push_back(endTuple);
            }
            index++;
        }
    }
    // processedIntervals.CODEPOINT 2

    if (startAndEndTuples.empty()) { return; }

    std::sort(startAndEndTuples.begin(), startAndEndTuples.end(),
              [](const auto& lhs, const auto& rhs) {
                  // sorts by time, then by end-ness, then by index
                  if (std::get<TIME_INDEX>(lhs) != std::get<TIME_INDEX>(rhs)) {
                      return std::get<TIME_INDEX>(lhs) < std::get<TIME_INDEX>(rhs);
                  } else if (std::get<IS_END_INDEX>(lhs) != std::get<IS_END_INDEX>(rhs)) {
                      return std::get<IS_END_INDEX>(lhs) < std::get<IS_END_INDEX>(rhs);
                  } else {
                      return std::get<INDEX_INDEX>(lhs) < std::get<INDEX_INDEX>(rhs);
                  }
              });

    // For code clarity below.
    typedef std::list<std::pair<std::optional<double>, int>> scopeType; // N.B. the word scope as used here refers to something else than scope used everywhere else in LCC
    constexpr int START_TIME_INDEX = 0;
    constexpr int END_TIME_INDEX = 1;
    constexpr int SCOPE_INDEX = 2;

    std::vector<std::tuple<double, double, scopeType>> intervalsWithScopes;

    scopeType startingScope;
    std::pair<std::optional<double>, int> base = {std::nullopt, -1};
    startingScope.push_back(base);
    std::tuple<double, double, scopeType> nextTuple(0, -1, startingScope);

    // processedIntervals.CODEPOINT 3
    for (auto& [time, value, index, isEnd] : startAndEndTuples) {
        // Now that we know the end time for the last interval, modify and push it.
        std::get<END_TIME_INDEX>(nextTuple) = time;
        intervalsWithScopes.push_back(nextTuple);
        // Then, modify the next scope as determined by the tuple we are processing.
        scopeType scope(std::get<SCOPE_INDEX>(nextTuple));  // copies
        std::pair<std::optional<double>, int> declaration = {std::optional<double>(value), index};
        if (!isEnd) {
            scope.push_front(declaration);
        } else {
            auto iter = find_if(scope.begin(), scope.end(),
                                [=](auto item) {
                                    return std::get<0>(item) == std::get<0>(declaration)
                                           && std::get<1>(item) == std::get<1>(declaration);
                                });
            if (iter == scope.end()) {
                error("Closing constant molecule count declaration that doesn't exist.");
            }
            scope.erase(iter);
        }
        nextTuple = {time, -1, scope};
    }
    std::get<END_TIME_INDEX>(nextTuple) = std::numeric_limits<double>::infinity();
    intervalsWithScopes.push_back(nextTuple);

    // processedIntervals.CODEPOINT 4

    processedIntervals.clear();

    for (auto& [startTime, endTime, scope] : intervalsWithScopes) {
        int largestIndex = std::numeric_limits<int>::min();
        std::optional<double> value(std::nullopt);
        for (auto& [tempValue, index] : scope) {
            if (index > largestIndex) {
                largestIndex = index;
                value = tempValue;
            }
        }
        std::tuple<double, double, std::optional<double>> processedInterval = {startTime, endTime, value};
        processedIntervals.push_back(processedInterval);
    }
}

// See wiki/Compiler Context/Implementation Notes/getIntervalPoints() Overview/
void FixedCountHandler::removeExtraIntervals() {
    auto iterator = processedIntervals.begin();
    while (iterator != processedIntervals.end()) {
        auto& [start, end, value] = *iterator;
        if (start == end) {
            iterator = processedIntervals.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

// See wiki/Compiler Context/Implementation Notes/getIntervalPoints() Overview/
void FixedCountHandler::mergeIntervals() {
    auto iterator = processedIntervals.begin();
    if (iterator == processedIntervals.end()) {
        return;
    }
    auto nextIterator = std::next(iterator);
    while (nextIterator != processedIntervals.end()) {
        auto& [start1, end1, value1] = *iterator;
        auto& [start2, end2, value2] = *nextIterator;
        if (value1 == value2) {
            std::tuple<double, double, std::optional<double>> newTuple(start1, end2, value1);

            processedIntervals.erase(nextIterator);  // Don't need to update nextIterator as we will reassign below.
            iterator = processedIntervals.erase(iterator);

            // We don't want to advance iterator as there may be more intervals with the same value.
            iterator = processedIntervals.insert(iterator, newTuple);
            nextIterator = std::next(iterator);
        } else {
            ++iterator;
            ++nextIterator;
        }
    }
}

// See wiki/Compiler Context/Implementation Notes/getIntervalPoints() Overview/
void FixedCountHandler::convertIntervals() {
    intervalPoints.clear();
    for (auto& [start, end, value] : processedIntervals) {
        intervalPoints[start] = value;
    }
}



/* MOLECULE */

Molecule::Molecule(Compartment* newCompartment, const std::string& newName, int newIndexInCompartment) :
        compartment(newCompartment),
        name(newName),
        indexInCompartment(newIndexInCompartment),
        initialCount(std::nullopt),
        fixedCountHandler(nullptr)
{
    if (newCompartment == nullptr) {
        error("newCompartment passed to Molecule::Molecule() constructor cannot be null.");
    }
    fixedCountHandler = new FixedCountHandler(this);
}

Molecule::Molecule(Compartment* newCompartment, const std::string& newName, int newIndexInCompartment,
                   double newInitialCount) :
        compartment(newCompartment),
        name(newName),
        indexInCompartment(newIndexInCompartment),
        initialCount(std::optional<double>(newInitialCount)),
        fixedCountHandler(nullptr)
{
    if (newCompartment == nullptr) {
        error("newCompartment passed to Molecule::Molecule() constructor cannot be null.");
    }
    fixedCountHandler = new FixedCountHandler(this);
}

Molecule::~Molecule() {
    delete fixedCountHandler;
}

Compartment* Molecule::getCompartment() const {
    return compartment;
}

int Molecule::getIndexInCompartment() const {
    return indexInCompartment;
}

const std::string& Molecule::getName() const {
    return name;
}

bool Molecule::hasInitialCount() const {
    return initialCount.has_value();
}

double Molecule::getInitialCount() const {
    if (initialCount.has_value()) {
        return initialCount.value();
    } else {
        error("Molecule " + name + " was asked for its initial count, but its initial count has not yet been specified.");
    }
}

void Molecule::setInitialCount(double newInitialCount) {
    initialCount = std::optional<double>(newInitialCount);
}

std::optional<double> Molecule::getBaseline() const {
    return fixedCountHandler->getBaseline();
}

const std::map<double, double>& Molecule::getChangePoints() const {
    return fixedCountHandler->getChangePoints();
}

const std::map<double, std::optional<double>>& Molecule::getIntervalPoints() const {
    return fixedCountHandler->getIntervalPoints();
}

/* REACTION */

Reaction::Reaction(Compartment* newCompartment, const std::string& newName) :
        compartment(newCompartment),
        name(newName),
        type(REACTION_TYPE::NOT_YET_DETERMINED),

        reactants(),
        products(),

        stoichiometry(),
        parameters()
{
    if (newCompartment == nullptr) {
        error("newCompartment passed to Reaction::Reaction() constructor cannot be null.");
    }
}

Reaction::Reaction(const Reaction& reaction) :
        compartment(reaction.compartment),
        name(reaction.name),
        type(reaction.type),

        reactants(reaction.reactants),
        products(reaction.products),

        stoichiometry(reaction.stoichiometry),
        parameters(reaction.parameters) {

}

Reaction::~Reaction() = default;

Compartment* Reaction::getCompartment() const {
    return compartment;
}

const std::string& Reaction::getName() const {
    return name;
}

REACTION_TYPE Reaction::getType() const {
    return type;
}

bool Reaction::canHaveType(REACTION_TYPE reactionType) const {
    if (reactionType == REACTION_TYPE::SU && parameters.size() == 1 && parameters.count(PARAM::K) == 1) {
        // Special handling for implied krev = 0 (i.e. only k specified). krev will be set to 0 when reaction type is assigned in setType().
        return true;
    }
    if (parameters.size() > requiredParams[reactionType].size()) {
        return false;
    } else {
        for (PARAM requiredParam : requiredParams[reactionType]) {
            if (parameters.count(requiredParam) == 0) {
                return false;
            }
        }
    }
    return true;
}

void Reaction::setType(REACTION_TYPE newType) {
    if (newType == REACTION_TYPE::SU && parameters.count(PARAM::KREV) == 0) {
        // TODO: wrap message behind compiler flags
        std::cout << "Warning: reaction " << this->getName() << " in compartment " << compartment->getName() << " was assumed to have implicit parameter krev = 0." << std::endl;
        parameters[PARAM::KREV] = 0.0;
    }
    type = newType;
}

const std::vector<Molecule*>& Reaction::getReactants() const {
    return reactants;
}

void Reaction::addReactant(Molecule* molecule, int stoichiometricCoefficient) {
    reactants.push_back(molecule);
    stoichiometry[molecule] = stoichiometricCoefficient;
}

const std::vector<Molecule*>& Reaction::getProducts() const {
    return products;
}

void Reaction::addProduct(Molecule* molecule, int stoichiometricCoefficient) {
    products.push_back(molecule);
    stoichiometry[molecule] = stoichiometricCoefficient;
}

bool Reaction::hasProtein() const {
    return protein.has_value();
}

Molecule* Reaction::getProtein() const {
    if (!protein.has_value()) {
        error("Reaction asked for protein, but has none.");
    }
    return protein.value();
}

void Reaction::setProtein(Molecule* molecule) {
    protein = std::optional<Molecule*>(molecule);
}

int Reaction::getStoichiometricCoefficient(Molecule* molecule) {
    // It is desired behavior that this function creates a 0 in the map and returns it
    // when molecule isn't already present in the reaction stoichiometry.
    // Hence this function is non-const.
    return stoichiometry[molecule];
}

bool Reaction::hasParameter(PARAM parameter) const {
    return parameters.count(parameter) > 0;
}

double Reaction::getParameterValue(PARAM parameter) const {
    return parameters.at(parameter);
}

const std::unordered_map<PARAM, double>& Reaction::getParameters() const {
    return parameters;
}

void Reaction::addParameter(PARAM parameterName, double value) {
    parameters[parameterName] = value;
}

/* ACTIVATION : REACTION */

Activation::Activation(Reaction* oldReaction, const std::string& newActivationReactionName, Molecule* newActivator) :
        Reaction(*oldReaction),
        activationReactionName(newActivationReactionName),
        activator(newActivator),
        activationParameters() {
    if (oldReaction->getType() != REACTION_TYPE::SU) {
        error("Converting reactions to activations is only supported for standard unregulated reactions.");
    }
}

Activation::~Activation() = default;

const std::string& Activation::getActivationReactionName() {
    return activationReactionName;
}

Molecule* Activation::getActivator() const {
    return activator;
}

bool Activation::hasActivationParameter(PARAM parameter) const {
    return activationParameters.count(parameter) > 0;
}

double Activation::getActivationParameterValue(PARAM parameter) const {
    return activationParameters.at(parameter);
}

void Activation::addActivationParameter(PARAM parameterName, double value) {
    activationParameters[parameterName] = value;
}

bool Activation::canHaveType(REACTION_TYPE reactionType) const {
    if (activationParameters.size() > requiredParams[reactionType].size()) {
        return false;
    } else {
        for (PARAM requiredParam : requiredParams[reactionType]) {
            if (activationParameters.count(requiredParam) == 0) {
                return false;
            }
        }
    }
    return true;
}

/* INHIBITION : REACTION */

Inhibition::Inhibition(Reaction* oldReaction, const std::string& newInhibitionReactionName, Molecule* newInhibitor) :
        Reaction(*oldReaction),
        inhibitionReactionName(newInhibitionReactionName),
        inhibitor(newInhibitor),
        inhibitionParameters() {
    if (oldReaction->getType() != REACTION_TYPE::SU) {
        error("Converting reactions to inhibitions is only supported for standard unregulated reactions.");
    }
}

Inhibition::~Inhibition() = default;

const std::string& Inhibition::getInhibitionReactionName() {
    return inhibitionReactionName;
}

Molecule* Inhibition::getInhibitor() const {
    return inhibitor;
}

bool Inhibition::hasInhibitionParameter(PARAM parameter) const {
    return inhibitionParameters.count(parameter) > 0;
}

double Inhibition::getInhibitionParameterValue(PARAM parameter) const {
    return inhibitionParameters.at(parameter);
}

void Inhibition::addInhibitionParameter(PARAM parameterName, double value) {
    inhibitionParameters[parameterName] = value;
}

bool Inhibition::canHaveType(REACTION_TYPE reactionType) const {
    if (inhibitionParameters.size() > requiredParams[reactionType].size()) {
        return false;
    } else {
        for (PARAM requiredParam : requiredParams[reactionType]) {
            if (inhibitionParameters.count(requiredParam) == 0) {
                return false;
            }
        }
    }
    return true;
}

/* COMPARTMENT */
Compartment::Compartment(Compartment* newParent, const std::string& newName, COMPARTMENT_TYPE newType) :
        parent(newParent),
        name(newName),
        type(newType),
        volume(LCC_DEFAULT_VOLUME),
        isSpatial(false),

        moleculeNameToIndex(),
        reactionNameToIndex(),
        molecules(),
        reactions(),

        children() {}

Compartment::Compartment(Compartment* newParent, const std::string& newName, COMPARTMENT_TYPE newType, double newVolume) :
        parent(newParent),
        name(newName),
        type(newType),
        volume(newVolume),
        isSpatial(false),

        moleculeNameToIndex(),
        reactionNameToIndex(),
        molecules(),
        reactions(),

        children() {}

Compartment::Compartment(Compartment* newParent, const std::string& newName, COMPARTMENT_TYPE newType, bool newIsSpatial) :
        parent(newParent),
        name(newName),
        type(newType),
        volume(LCC_DEFAULT_VOLUME),
        isSpatial(newIsSpatial),

        moleculeNameToIndex(),
        reactionNameToIndex(),
        molecules(),
        reactions(),

        children() {}

Compartment::Compartment(Compartment* newParent, const std::string& newName, COMPARTMENT_TYPE newType, double newVolume,
                         bool newIsSpatial) :
        parent(newParent),
        name(newName),
        type(newType),
        volume(newVolume),
        isSpatial(newIsSpatial),

        moleculeNameToIndex(),
        reactionNameToIndex(),
        molecules(),
        reactions(),

        children() {}

Compartment::~Compartment() {
    for (Molecule* molecule : molecules) {
        delete molecule;
    }
    for (Reaction* reaction : reactions) {
        delete reaction;
    }
    for (Compartment* child : children) {
        delete child;
    }
}

Compartment* Compartment::getParent() const {
    return parent;
}

const std::string& Compartment::getName() const {
    return name;
}

COMPARTMENT_TYPE Compartment::getType() const {
    return type;
}

double Compartment::getVolume() const {
    return volume;
}

bool Compartment::isSpatialCompartment() const {
    return isSpatial;
}

const std::vector<Compartment*>& Compartment::getChildren() const {
    return children;
}

void Compartment::addChild(Compartment* child) {
    children.push_back(child);
}

const std::vector<Molecule*>& Compartment::getMolecules() const {
    return molecules;
}

bool Compartment::hasMolecule(const std::string& nameToSearch) const {
    return moleculeNameToIndex.count(nameToSearch) > 0;
}

Molecule* Compartment::getMolecule(const std::string& nameToFind) const {
    return molecules[moleculeNameToIndex.at(nameToFind)];
}

void Compartment::addMolecule(Molecule* molecule) {
    molecules.push_back(molecule);
    moleculeNameToIndex[molecule->getName()] = molecules.size() - 1;
}

// See wiki/Compiler Context/Interface Notes/processMoleculeAssignments() Well-Formed Inputs/ for information what inputs we expect.
// After understanding the structure of inputs, this function should become clear.
void Compartment::processMoleculeAssignment(SymbolNode* assignmentNode) {
    assignmentNode->assertSymbol(SYMBOL::ASSIGNMENT, "Symbol node other than ASSIGNMENT type passed to processMoleculeAssignment.");
    assignmentNode->getRight()->assertNodeType(NODE::NUMBER_NODE, "Only number nodes supported for molecule assignments at present.");
    NumberNode* valueNode = dynamic_cast<NumberNode*>(assignmentNode->getRight());
    double value = valueNode->getSIValue();

    switch (assignmentNode->getLeft()->getNodeType()) {
        case NODE::IDENTIFIER_NODE:
        case NODE::CHEMICAL_NODE: {
            std::string moleculeName;
            if (assignmentNode->getLeft()->getNodeType() == NODE::IDENTIFIER_NODE) {
                IdentifierNode* moleculeIdentifier = dynamic_cast<IdentifierNode*>(assignmentNode->getLeft());
                moleculeName = moleculeIdentifier->getName();
            } else {
                // chemical node
                ChemicalNode* chemicalNode = dynamic_cast<ChemicalNode*>(assignmentNode->getLeft());
                moleculeName = chemicalNode->getFormula();
            }

            if (this->hasMolecule(moleculeName)) {
                Molecule* molecule = this->getMolecule(moleculeName);
                molecule->setInitialCount(value);
            } else {
                Molecule* molecule = new Molecule(this, moleculeName, molecules.size(), value);
                this->addMolecule(molecule);
            }
            // TODO: wrap behind compiler flags
            std::cout << "Warning: assignment of molecule " << moleculeName << " implicitly refers to initial count. Consider making explicit with " << moleculeName << "[0], or using " << moleculeName << "[:] if molecule is meant to be kept constant." << std::endl;
            break;
        }
        case NODE::INDEX_NODE: {
            IndexNode* indexNode = dynamic_cast<IndexNode*>(assignmentNode->getLeft());

            std::string moleculeName;
            if (indexNode->getLeft()->getNodeType() == NODE::IDENTIFIER_NODE) {
                IdentifierNode* moleculeIdentifier = dynamic_cast<IdentifierNode*>(indexNode->getLeft());
                moleculeName = moleculeIdentifier->getName();
            } else {
                // chemical node
                ChemicalNode* chemicalNode = dynamic_cast<ChemicalNode*>(indexNode->getLeft());
                moleculeName = chemicalNode->getFormula();
            }

            Molecule* molecule = nullptr;
            if (this->hasMolecule(moleculeName)) {
                molecule = this->getMolecule(moleculeName);
            } else {
                molecule = new Molecule(this, moleculeName, molecules.size());
                this->addMolecule(molecule);
            }

            switch (indexNode->getRight()->getNodeType()) {
                case NODE::NUMBER_NODE: {
                    NumberNode* timeNode = dynamic_cast<NumberNode*>(indexNode->getRight());
                    double time = timeNode->getSIValue();
                    FixedCountHandler::getFixedCountHandler(molecule)->addChangePoint(time, value);
                    break;
                }
                case NODE::SYMBOL_NODE: {
                    SymbolNode* colon = dynamic_cast<SymbolNode*>(indexNode->getRight());
                    colon->assertSymbol(SYMBOL::COLON, "Index node has SYMBOL right child, but it's symbol is not a COLON.");

                    double startTime, endTime;
                    if (colon->getLeft()->getNodeType() == NODE::NUMBER_NODE) {
                        NumberNode* startTimeNode = dynamic_cast<NumberNode*>(colon->getLeft());
                        startTime = startTimeNode->getSIValue();
                    } else {
                        colon->getLeft()->assertNodeType(NODE::AST_NODE, "Colon node has left child other than AST_NODE or NUMBER_NODE."); // needs to change to evaluate numerical expressions
                        startTime = 0;
                    }
                    if (colon->getRight()->getNodeType() == NODE::NUMBER_NODE) {
                        NumberNode* endTimeNode = dynamic_cast<NumberNode*>(colon->getRight());
                        endTime = endTimeNode->getSIValue();
                    } else {
                        colon->getRight()->assertNodeType(NODE::AST_NODE, "Colon node has right child other than AST_NODE or NUMBER_NODE."); // needs to change to evaluate numerical expressions
                        endTime = std::numeric_limits<double>::infinity();
                    }

                    FixedCountHandler::getFixedCountHandler(molecule)->addInterval(value, startTime, endTime);
                    break;
                }
                default:
                    error("Index node with right child other than NUMBER or SYMBOL type.");
            }
            break;
        }
        default:
            error("processMoleculeAssignment ASSIGNMENT node has left child other than IDENTIFIER, CHEMICAL or INDEX.");

    }

}

const std::vector<Reaction*>& Compartment::getReactions() const {
    return reactions;
}

bool Compartment::hasReaction(const std::string& nameToSearch) const {
    return reactionNameToIndex.count(nameToSearch) > 0;
}

Reaction* Compartment::getReaction(const std::string& nameToFind) const {
    return reactions[reactionNameToIndex.at(nameToFind)];
}

void Compartment::addReaction(Reaction* reaction) {
    reactions.push_back(reaction);
    reactionNameToIndex[reaction->getName()] = reactions.size() - 1;
}

void Compartment::removeReaction(Reaction* reaction) {
    int index = reactionNameToIndex[reaction->getName()];
    reactions.erase(reactions.begin() + index);
    reactionNameToIndex.erase(reaction->getName());
    for (int i = index; i < reactions.size(); i++) {
        reactionNameToIndex[reactions[i]->getName()]--;
    }
}


/*
 *             INPUT ---->       reactionNode
 *                           /                  \        next statement              next statement
 *                 reaction identifier       parameter  --------------->  parameter  -------------> etc.
 *                                           assignment                   assignment
 *                                          /       \                    /       \
 *                                   parameter    parameter       parameter    parameter
 *                                   identifier   expression      identifier   expression
 *
 */
void Compartment::processReaction(KeywordNode* reactionNode, bool isInProtein, const std::string& proteinName) {
    reactionNode->assertKeyword(KEYWORD::REACTION, "KeywordNode other than REACTION type passed to processReaction (type passed: " + keywordTypeToText[reactionNode->getKeyword()] + ").");

    reactionNode->getLeft()->assertNodeType(NODE::IDENTIFIER_NODE, "Reaction node with left child other than IDENTIFIER type passed to processReaction.");
    IdentifierNode* reactionIdentifierNode = dynamic_cast<IdentifierNode*>(reactionNode->getLeft());
    std::string reactionName = reactionIdentifierNode->getName();

    ASTNode* parameterAssignmentNode = reactionNode->getRight();

    Reaction* reaction = new Reaction(this, reactionName);

    parameterAssignmentNode->assertNodeType(NODE::AST_NODE, "Syntax error: reaction " + reactionName + " has no parameters.", true);

    while (true) {
        parameterAssignmentNode->assertNodeType(NODE::SYMBOL_NODE, "Reaction node with parameter node other than SYMBOL type passed to processReaction.");
        SymbolNode* parameterAssignment = dynamic_cast<SymbolNode*>(parameterAssignmentNode);
        parameterAssignment->assertSymbol(SYMBOL::ASSIGNMENT, "Reaction node with parameter symbol node other than ASSIGNMENT type passed to processReaction.");

        parameterAssignment->getLeft()->assertNodeType(NODE::PARAM_NODE, "Parameter assignment node with left child other than PARAM type passed to processReaction.");
        ParamNode* parameterIdentifierNode = dynamic_cast<ParamNode*>(parameterAssignment->getLeft());

        if (parameterIdentifierNode->getParamType() == PARAM::EQUATION) {
            if (reaction->hasParameter(PARAM::EQUATION)) {
                error("Reaction " + reactionName + " has equation defined more than once.");
            }
            parameterAssignment->getRight()->assertNodeType(NODE::SYMBOL_NODE, "Reaction eq parameter assignment does not have symbol node right child.");
            SymbolNode* rightArrowOrInhibition = dynamic_cast<SymbolNode*>(parameterAssignment->getRight());
            switch (rightArrowOrInhibition->getSymbol()) {
                case SYMBOL::FORWARD: {
                    // --> = normal reaction or activation
                    if (checkForActivation(rightArrowOrInhibition)) {
                        this->processActivation(reactionName, reaction, parameterAssignment);
                        return;
                    } else {
                        // Normal reaction
                        this->processReactants(rightArrowOrInhibition->getLeft(), reaction);
                        this->processProducts(rightArrowOrInhibition->getRight(), reaction);
                    }
                    break;
                }
                case SYMBOL::INHIBITION: {
                    this->processInhibition(reactionName, reaction, parameterAssignment);
                    return;
                }
                default:
                    error("Reaction eq parameter assignment has symbol node right child, but that symbol is not a --> or --|.");
            }

        } else {
            PARAM parameter = parameterIdentifierNode->getParamType();

            if (validReactionParameters.count(parameter) == 0) {
                error("Reaction " + reactionName + " has invalid parameter " + paramTypeToText[parameter] + ".");
            } else if (reaction->hasParameter(parameter)) {
                error("Reaction " + reactionName + " has parameter " + paramTypeToText[parameter] + " defined more than once.");
            }

            parameterAssignment->getRight()->assertNodeType(NODE::NUMBER_NODE, "Only number nodes supported for reaction parameter values at present.");
            NumberNode* numberNode = dynamic_cast<NumberNode*>(parameterAssignment->getRight());
            double value = numberNode->getSIValue();
            reaction->addParameter(parameter, value);
        }
        if (!parameterAssignmentNode->hasNextStatement) {
            break;
        } else {
            parameterAssignmentNode = parameterAssignmentNode->getNextStatement();
        }
    }

    // TODO: more detailed errors when reaction type inference fails
    if (!isInProtein) {
        for (REACTION_TYPE possibleType : {REACTION_TYPE::SU}) {
            if (reaction->canHaveType(possibleType)) {
                reaction->setType(possibleType);
                break;
            }
        }
        if (reaction->getType() == REACTION_TYPE::NOT_YET_DETERMINED) {
            error("Reaction type of reaction " + reaction->getName() + " cannot be determined. It likely has not enough or conflicting parameters.");
        }
    } else {
        if (this->hasMolecule(proteinName)) {
            Molecule* protein = this->getMolecule(proteinName);
            reaction->setProtein(protein);
        } else {
            Molecule* protein = new Molecule(this, proteinName, molecules.size());
            this->addMolecule(protein);
            reaction->setProtein(protein);
        }
        for (REACTION_TYPE possibleType : {REACTION_TYPE::ESU, REACTION_TYPE::MMU}) {
            if (reaction->canHaveType(possibleType)) {
                reaction->setType(possibleType);
                break;
            }
        }
        if (reaction->getType() == REACTION_TYPE::NOT_YET_DETERMINED) {
            error("Reaction type of reaction " + reaction->getName() + " cannot be determined. It likely has not enough or conflicting parameters.");
        }
    }

    this->addReaction(reaction);
    std::cout << "Added reaction " << reaction->getName() << " to compartment " << this->getName() << std::endl;
}

void Compartment::processReactants(ASTNode* equationLHS, Reaction* reaction) {
    // TODO: check that identifiers make sense (i.e. dont refer to random other stuff)
    switch (equationLHS->getNodeType()) {
        case NODE::IDENTIFIER_NODE: {
            IdentifierNode* identifierNode = dynamic_cast<IdentifierNode*>(equationLHS);
            std::string moleculeName = identifierNode->getName();
            if (this->hasMolecule(moleculeName)) {
                Molecule* molecule = this->getMolecule(moleculeName);
                reaction->addReactant(molecule, -1);
            } else {
                Molecule* molecule = new Molecule(this, moleculeName, molecules.size());
                this->addMolecule(molecule);
                reaction->addReactant(molecule, -1);
            }
            break;
        }
        case NODE::CHEMICAL_NODE: {
            ChemicalNode* chemicalNode = dynamic_cast<ChemicalNode*>(equationLHS);
            std::string moleculeName = chemicalNode->getFormula();
            if (this->hasMolecule(moleculeName)) {
                Molecule* molecule = this->getMolecule(moleculeName);
                reaction->addReactant(molecule, -1);
            } else {
                Molecule* molecule = new Molecule(this, moleculeName, molecules.size());
                this->addMolecule(molecule);
                reaction->addReactant(molecule, -1);
            }
            break;
        }
        case NODE::SYMBOL_NODE: {
            SymbolNode* symbolNode = dynamic_cast<SymbolNode*>(equationLHS);
            switch (symbolNode->getSymbol()) {
                case SYMBOL::ADD: {
                    this->processReactants(symbolNode->getLeft(), reaction);
                    this->processReactants(symbolNode->getRight(), reaction);
                    break;
                }
                case SYMBOL::MULTIPLY: {
                    symbolNode->getLeft()->assertNodeType(NODE::NUMBER_NODE, "LHS of reaction " + reaction->getName() + " has multiplication node with left child other than NUMBER type.");
                    symbolNode->getRight()->assertNodeType({NODE::IDENTIFIER_NODE, NODE::CHEMICAL_NODE}, "LHS of reaction " + reaction->getName() + " has multiplication node with right child other than IDENTIFIER or CHEMICAL type.");

                    NumberNode* coefficientNode = dynamic_cast<NumberNode*>(symbolNode->getLeft());

                    std::string moleculeName;
                    if (symbolNode->getRight()->getNodeType() == NODE::IDENTIFIER_NODE) {
                        IdentifierNode* right = dynamic_cast<IdentifierNode*>(symbolNode->getRight());
                        moleculeName = right->getName();
                    } else {
                        // Chemical node
                        ChemicalNode* right = dynamic_cast<ChemicalNode*>(symbolNode->getRight());
                        moleculeName = right->getFormula();
                    }

                    int stoichiometricCoefficient = -1 * (int)std::round(coefficientNode->getNum());

                    if (this->hasMolecule(moleculeName)) {
                        Molecule* molecule = this->getMolecule(moleculeName);
                        reaction->addReactant(molecule, stoichiometricCoefficient);
                    } else {
                        Molecule* molecule = new Molecule(this, moleculeName, molecules.size());
                        this->addMolecule(molecule);
                        reaction->addReactant(molecule, stoichiometricCoefficient);
                    }
                    break;
                }
                default: {
                    error("LHS of reaction " + reaction->getName() + " has symbol other than + or *");
                }
            }
            break;
        }
        default: {
            error("LHS of reaction " + reaction->getName() + " has node other than IDENTIFIER, CHEMICAL or SYMBOL.");
        }
    }
}

void Compartment::processProducts(ASTNode* equationRHS, Reaction* reaction) {
    switch (equationRHS->getNodeType()) {
        case NODE::IDENTIFIER_NODE: {
            IdentifierNode* identifierNode = dynamic_cast<IdentifierNode*>(equationRHS);
            std::string moleculeName = identifierNode->getName();
            if (this->hasMolecule(moleculeName)) {
                Molecule* molecule = this->getMolecule(moleculeName);
                reaction->addProduct(molecule, 1);
            } else {
                Molecule* molecule = new Molecule(this, moleculeName, molecules.size());
                this->addMolecule(molecule);
                reaction->addProduct(molecule, 1);
            }
            break;
        }
        case NODE::CHEMICAL_NODE: {
            ChemicalNode* chemicalNode = dynamic_cast<ChemicalNode*>(equationRHS);
            std::string moleculeName = chemicalNode->getFormula();
            if (this->hasMolecule(moleculeName)) {
                Molecule* molecule = this->getMolecule(moleculeName);
                reaction->addProduct(molecule, 1);
            } else {
                Molecule* molecule = new Molecule(this, moleculeName, molecules.size());
                this->addMolecule(molecule);
                reaction->addProduct(molecule, 1);
            }
            break;
        }
        case NODE::SYMBOL_NODE: {
            SymbolNode* symbolNode = dynamic_cast<SymbolNode*>(equationRHS);
            switch (symbolNode->getSymbol()) {
                case SYMBOL::ADD: {
                    this->processProducts(symbolNode->getLeft(), reaction);
                    this->processProducts(symbolNode->getRight(), reaction);
                    break;
                }
                case SYMBOL::MULTIPLY: {
                    symbolNode->getLeft()->assertNodeType(NODE::NUMBER_NODE, "RHS of reaction " + reaction->getName() + " has multiplication node with left child other than NUMBER type.");
                    symbolNode->getRight()->assertNodeType({NODE::IDENTIFIER_NODE, NODE::CHEMICAL_NODE}, "RHS of reaction " + reaction->getName() + " has multiplication node with right child other than IDENTIFIER or CHEMICAL type.");

                    NumberNode* coefficientNode = dynamic_cast<NumberNode*>(symbolNode->getLeft());

                    std::string moleculeName;
                    if (symbolNode->getRight()->getNodeType() == NODE::IDENTIFIER_NODE) {
                        IdentifierNode* right = dynamic_cast<IdentifierNode*>(symbolNode->getRight());
                        moleculeName = right->getName();
                    } else {
                        // Chemical node
                        ChemicalNode* right = dynamic_cast<ChemicalNode*>(symbolNode->getRight());
                        moleculeName = right->getFormula();
                    }

                    int stoichiometricCoefficient = (int)std::round(coefficientNode->getNum());

                    if (this->hasMolecule(moleculeName)) {
                        Molecule* molecule = this->getMolecule(moleculeName);
                        reaction->addProduct(molecule, stoichiometricCoefficient);
                    } else {
                        Molecule* molecule = new Molecule(this, moleculeName, molecules.size());
                        this->addMolecule(molecule);
                        reaction->addProduct(molecule, stoichiometricCoefficient);
                    }
                    break;
                }
                default: {
                    error("RHS of reaction " + reaction->getName() + " has symbol other than + or *");
                }
            }
            break;
        }
        default: {
            error("RHS of reaction " + reaction->getName() + " has node other than IDENTIFIER, CHEMICAL or SYMBOL.");
        }
    }
}

bool Compartment::checkForActivation(SymbolNode* rightArrowNode) const {
    if (rightArrowNode->getLeft()->getNodeType() != NODE::IDENTIFIER_NODE && rightArrowNode->getLeft()->getNodeType() != NODE::CHEMICAL_NODE) {
        return false;
    } else if (rightArrowNode->getRight()->getNodeType() != NODE::IDENTIFIER_NODE) {
        return false;
    } else {
        IdentifierNode* rightIdentifier = dynamic_cast<IdentifierNode*>(rightArrowNode->getRight());
        // TODO: check left identifiers make sense
        if (this->hasReaction(rightIdentifier->getName())) {
            return true;
        } else {
            return false;
        }
    }
}

// inProgressReaction is the reaction we were building before we discovered it was an activation reaction.
void Compartment::processActivation(const std::string& activationReactionName, Reaction* inProgressReaction, SymbolNode* equationAssignmentNode) {
    SymbolNode* rightArrowNode = dynamic_cast<SymbolNode*>(equationAssignmentNode->getRight());

    IdentifierNode* rightIdentifier = dynamic_cast<IdentifierNode*>(rightArrowNode->getRight());

    std::string activatedReactionName = rightIdentifier->getName();
    Reaction* oldReaction = this->getReaction(activatedReactionName);
    // remove old reaction, which was not of activated type
    this->removeReaction(oldReaction);

    std::string activatorName;
    if (rightArrowNode->getLeft()->getNodeType() == NODE::IDENTIFIER_NODE) {
        IdentifierNode* left = dynamic_cast<IdentifierNode*>(rightArrowNode->getLeft());
        activatorName = left->getName();
    } else {
        // Chemical node
        ChemicalNode* left = dynamic_cast<ChemicalNode*>(rightArrowNode->getLeft());
        activatorName = left->getFormula();
    }

    Molecule* activator = nullptr;
    if (this->hasMolecule(activatorName)) {
        activator = this->getMolecule(activatorName);
    } else {
        Molecule* molecule = new Molecule(this, activatorName, molecules.size());
        this->addMolecule(molecule);
        activator = molecule;
    }

    Activation* newReaction = new Activation(oldReaction, activationReactionName, activator);
    for (auto& [parameter, value] : inProgressReaction->getParameters()) {
        newReaction->addActivationParameter(parameter, value);
    }
    delete oldReaction;
    delete inProgressReaction;

    // May be more parameters to extract
    if (equationAssignmentNode->hasNextStatement) {
        ASTNode* parameterAssignmentNode = equationAssignmentNode->getNextStatement();
        while (true) {
            parameterAssignmentNode->assertNodeType(NODE::SYMBOL_NODE, "Reaction node with parameter node other than SYMBOL type passed to processActivation.");
            SymbolNode* parameterAssignment = dynamic_cast<SymbolNode*>(parameterAssignmentNode);
            parameterAssignment->assertSymbol(SYMBOL::ASSIGNMENT, "Reaction node with parameter symbol node other than ASSIGNMENT type passed to processActivation.");

            parameterAssignment->getLeft()->assertNodeType(NODE::PARAM_NODE, "Parameter assignment node with left child other than PARAM type passed to processActivation.");
            ParamNode* parameterIdentifierNode = dynamic_cast<ParamNode*>(parameterAssignment->getLeft());

            if (parameterIdentifierNode->getParamType() == PARAM::EQUATION) {
                error("Reaction " + activationReactionName + " has equation defined more than once.");
            } else {
                PARAM parameter = parameterIdentifierNode->getParamType();

                if (validReactionParameters.count(parameter) == 0) {
                    error("Reaction " + activationReactionName + " has invalid parameter " + paramTypeToText[parameter] + ".");
                } else if (newReaction->hasActivationParameter(parameter)) {
                    error("Reaction " + activationReactionName + " has parameter " + paramTypeToText[parameter] +
                          " defined more than once.");
                }

                parameterAssignment->getRight()->assertNodeType(NODE::NUMBER_NODE,
                                                                "Only number nodes supported for reaction parameter values at present.");
                NumberNode* numberNode = dynamic_cast<NumberNode*>(parameterAssignment->getRight());
                double value = numberNode->getSIValue();
                newReaction->addActivationParameter(parameter, value);
            }
            if (!parameterAssignmentNode->hasNextStatement) {
                break;
            } else {
                parameterAssignmentNode = parameterAssignmentNode->getNextStatement();
            }
        }
    }

    for (REACTION_TYPE possibleType : {REACTION_TYPE::SAA}) {
        if (newReaction->canHaveType(possibleType)) {
            newReaction->setType(possibleType);
            break;
        }
    }
    if (newReaction->getType() == REACTION_TYPE::NOT_YET_DETERMINED) {
        error("Reaction type of reaction " + newReaction->getActivationReactionName() + " cannot be determined. It likely has not enough or conflicting parameters.");
    }
    this->addReaction(newReaction);
    std::cout << "Reaction " << newReaction->getActivationReactionName() << " caused reaction " << newReaction->getName() << " to become an activation reaction in compartment " << this->getName() << std::endl;
}

// inProgressReaction is the reaction we were building before we discovered it was an inhibition reaction.
void Compartment::processInhibition(const std::string& inhibitionReactionName, Reaction* inProgressReaction,
                                    SymbolNode* equationAssignmentNode) {
    SymbolNode* inhibitionNode = dynamic_cast<SymbolNode*>(equationAssignmentNode->getRight());
    inhibitionNode->getLeft()->assertNodeType({NODE::IDENTIFIER_NODE, NODE::CHEMICAL_NODE}, "Inhibition " + inhibitionReactionName + " has left child that is not a CHEMICAL or IDENTIFIER node.");
    inhibitionNode->getRight()->assertNodeType(NODE::IDENTIFIER_NODE, "Inhibition " + inhibitionReactionName + " has right child that is not an IDENTIFIER node.");

    IdentifierNode* rightIdentifier = dynamic_cast<IdentifierNode*>(inhibitionNode->getRight());
    std::string inhibitedReactionName = rightIdentifier->getName();
    if (!this->hasReaction(inhibitedReactionName)) {
        error("Inhibition " + inhibitionReactionName + " inhibitions reaction " + inhibitedReactionName + ", but this reaction does not exist.");
    }

    Reaction* oldReaction = this->getReaction(inhibitedReactionName);
    // remove old reaction, which was not of activated type
    this->removeReaction(oldReaction);

    std::string inhibitorName;
    if (inhibitionNode->getLeft()->getNodeType() == NODE::IDENTIFIER_NODE) {
        IdentifierNode* left = dynamic_cast<IdentifierNode*>(inhibitionNode->getLeft());
        inhibitorName = left->getName();
    } else {
        // Chemical node
        ChemicalNode* left = dynamic_cast<ChemicalNode*>(inhibitionNode->getLeft());
        inhibitorName = left->getFormula();
    }

    Molecule* inhibitor = nullptr;
    if (this->hasMolecule(inhibitorName)) {
        inhibitor = this->getMolecule(inhibitorName);
    } else {
        Molecule* molecule = new Molecule(this, inhibitorName, molecules.size());
        this->addMolecule(molecule);
        inhibitor = molecule;
    }

    Inhibition* newReaction = new Inhibition(oldReaction, inhibitionReactionName, inhibitor);
    for (auto& [parameter, value] : inProgressReaction->getParameters()) {
        newReaction->addInhibitionParameter(parameter, value);
    }
    delete oldReaction;
    delete inProgressReaction;

    // may be more parameters to extract
    if (equationAssignmentNode->hasNextStatement) {
        ASTNode* parameterAssignmentNode = equationAssignmentNode->getNextStatement();
        while (true) {
            parameterAssignmentNode->assertNodeType(NODE::SYMBOL_NODE, "Reaction node with parameter node other than SYMBOL type passed to processInhibition.");
            SymbolNode* parameterAssignment = dynamic_cast<SymbolNode*>(parameterAssignmentNode);
            parameterAssignment->assertSymbol(SYMBOL::ASSIGNMENT, "Reaction node with parameter symbol node other than ASSIGNMENT type passed to processInhibition.");

            parameterAssignment->getLeft()->assertNodeType(NODE::PARAM_NODE, "Parameter assignment node with left child other than PARAM type passed to processInhibition.");
            ParamNode* parameterIdentifierNode = dynamic_cast<ParamNode*>(parameterAssignment->getLeft());

            if (parameterIdentifierNode->getParamType() == PARAM::EQUATION) {
                error("Reaction " + inhibitionReactionName + " has equation defined more than once.");
            } else {
                PARAM parameter = parameterIdentifierNode->getParamType();

                if (validReactionParameters.count(parameter) == 0) {
                    error("Reaction " + inhibitionReactionName + " has invalid parameter " + paramTypeToText[parameter] + ".");
                } else if (newReaction->hasInhibitionParameter(parameter)) {
                    error("Reaction " + inhibitionReactionName + " has parameter " + paramTypeToText[parameter] +
                          " defined more than once.");
                }

                parameterAssignment->getRight()->assertNodeType(NODE::NUMBER_NODE,
                                                                "Only number nodes supported for reaction parameter values at present.");
                NumberNode* numberNode = dynamic_cast<NumberNode*>(parameterAssignment->getRight());
                double value = numberNode->getSIValue();
                newReaction->addInhibitionParameter(parameter, value);
            }
            if (!parameterAssignmentNode->hasNextStatement) {
                break;
            } else {
                parameterAssignmentNode = parameterAssignmentNode->getNextStatement();
            }
        }
    }

    for (REACTION_TYPE possibleType : {REACTION_TYPE::SAI}) {
        if (newReaction->canHaveType(possibleType)) {
            newReaction->setType(possibleType);
            break;
        }
    }
    if (newReaction->getType() == REACTION_TYPE::NOT_YET_DETERMINED) {
        error("Reaction type of reaction " + newReaction->getInhibitionReactionName() + " cannot be determined. It likely has not enough or conflicting parameters.");
    }
    this->addReaction(newReaction);
    std::cout << "Reaction " << newReaction->getInhibitionReactionName() << " caused reaction " << newReaction->getName() << " to become an inhibition reaction in compartment " << this->getName() << std::endl;
}

/*
 *             INPUT ---->       proteinNode
 *                           /                  \       next statement             next statement
 *                 protein identifier        reaction  --------------->  reaction  -------------> etc.
 *                                          /       \                    /       \
 *                                       ...........................................
 *
 */
void Compartment::processProtein(KeywordNode* proteinNode) {
    proteinNode->assertKeyword(KEYWORD::PROTEIN, "KeywordNode with type other than PROTEIN passed to processProtein.");

    proteinNode->getLeft()->assertNodeType({NODE::IDENTIFIER_NODE, NODE::CHEMICAL_NODE}, "Protein node has left child other than IDENTIFIER or CHEMICAL type");
    std::string proteinName;
    if (proteinNode->getLeft()->getNodeType() == NODE::IDENTIFIER_NODE) {
        IdentifierNode* proteinIdentifier = dynamic_cast<IdentifierNode*>(proteinNode->getLeft());
        proteinName = proteinIdentifier->getName();
    } else {
        ChemicalNode* proteinIdentifier = dynamic_cast<ChemicalNode*>(proteinNode->getLeft());
        proteinName = proteinIdentifier->getFormula();
    }

    ASTNode* nodeToProcess = proteinNode->getRight();

    while (true) {
        nodeToProcess->assertNodeType(NODE::KEYWORD_NODE, "Protein statement other than KEYWORD type.");
        KeywordNode* keywordToProcess = dynamic_cast<KeywordNode*>(nodeToProcess);
        keywordToProcess->assertKeyword(KEYWORD::REACTION, "Protein KEYWORD statement other than REACTION type.");
        this->processReaction(keywordToProcess, true, proteinName);
        if (!nodeToProcess->hasNextStatement) {
            break;
        } else {
            nodeToProcess = nodeToProcess->getNextStatement();
        }
    }
}

Simulation::Simulation(const std::string& newName) :
        name(newName),
        globalCompartment(new Compartment(nullptr, "global", COMPARTMENT_TYPE::NON_SPATIAL, LCC_DEFAULT_VOLUME)) {
}

Simulation::~Simulation() {
    delete globalCompartment;
}

const std::string& Simulation::getName() const {
    return name;
}

Compartment* Simulation::getGlobalCompartment() const {
    return globalCompartment;
}

void Simulation::buildContext(ASTNode* node) {
    NODE nodeType = node->getNodeType();

    switch (nodeType) {
        // reaction statements
        case NODE::KEYWORD_NODE: {
            KeywordNode* keyword = dynamic_cast<KeywordNode*>(node);
            switch (keyword->getKeyword()) {
                case KEYWORD::REACTION:
                    globalCompartment->processReaction(keyword);
                    break;
                case KEYWORD::PROTEIN:
                    globalCompartment->processProtein(keyword);
                    break;
                default:
                    error("KeywordNode other than REACTION or PROTEIN in buildContext.");
            }
            break;
        }
            // assignment statements
        case NODE::SYMBOL_NODE: {
            SymbolNode* symbol = dynamic_cast<SymbolNode*>(node);
            globalCompartment->processMoleculeAssignment(symbol);
            break;
        }
        default: {
            fprintf(stderr, "\'Default\' during buildContext().\n");
            break;
        }
    }
}


void Simulation::buildSimulation(ASTNode* tree) {
    std::cout << "Building simulation..." << std::endl;
    // AST Traversal
    std::stack<ASTNode*> unvisited;
    ASTNode* firstStatement = tree;
    unvisited.push(firstStatement);

    while (!unvisited.empty()) {
        ASTNode* node = unvisited.top();
        unvisited.pop();
        if (!node->visited) {
            node->visited = true;
            buildContext(node);
            if (node->hasNextStatement) {
                unvisited.push(node->getNextStatement());
            }
        }
    }
    std::cout << "+ simulation successfully built!" << "\n" << std::endl;
}

}

int main(int argc, char* argv[]) {
    using namespace lcc;
    const char* fileName = argv[1];
    char* input = readFile(fileName);  // needs freeing
    std::string inputName(fileName);
    size_t lastIndex = inputName.find_last_of(".lpp");
    inputName = inputName.substr(0, lastIndex - LPP_FILENAME_OFFSET);
    ErrorCollector collect;
    Tokenizer* tokenizer = new Tokenizer(input, &collect);
    tokenizer->setFileSize(getFileSize(argv[1])) ;
    std::pair<Tokenizer::Token*, Tokenizer::Token*> headAndTail = tokenizer->tokenize(input);  // needs freeing
    Tokenizer::Token* head = headAndTail.first;
    Tokenizer::Token* tail = headAndTail.second;
    tokenizer->findIdentifiers(head);
    tokenizer->findChemicals(head);
    
    std::string path = fileName;
    std::string directory(fileName);
    size_t realFileNameStartingIndex = path.find_last_of('/') + 1;
    std::string realFileName(fileName);
    if (realFileNameStartingIndex == std::string::npos) {
        // already in same directory
        realFileNameStartingIndex = 0;
        directory = "./";
    }
    else {
        directory = directory.substr(0, realFileNameStartingIndex);
        realFileName = realFileName.substr(realFileNameStartingIndex);
    }

    /* new master file (imports replaced with code in those files) as a single merged token stream */
    FileNode* masterFile = tokenizer->linkImports(realFileName, directory, head, tail);
    head = masterFile->getFileHead();
    tail = masterFile->getFileTail();

    // Parser* parser = new Parser(head);
    // ASTNode* tree = parser->parse();  // needs freeing
    // Simulation* simulation = new Simulation("New Simulation");
    // simulation->buildSimulation(tree);
    // Writer* writer = new Writer();
    // writer->writeSimulation(simulation);
    // DiagramWriter* diagramWriter = new DiagramWriter();
    // diagramWriter->buildGraph(simulation->getGlobalCompartment());
    // delete diagramWriter;
    // delete writer;
    // delete simulation;
    // delete tree;
    // delete parser;
    // delete head;
    // delete tokenizer;
    // delete[] input;
}