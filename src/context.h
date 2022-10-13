#pragma once

#include <string>
#include <vector>
#include <set>
#include <list>
#include <unordered_map>
#include "ast.h"
#include "scope.h"

namespace lcc {

enum class REACTION_TYPE {
    NOT_YET_DETERMINED,
    STANDARD_UNREGULATED, SU = STANDARD_UNREGULATED,
    STANDARD_ALLOSTERIC_INHIBITION, SAI = STANDARD_ALLOSTERIC_INHIBITION,
    STANDARD_ALLOSTERIC_ACTIVATION, SAA = STANDARD_ALLOSTERIC_ACTIVATION,
    ENZYMATIC_STANDARD_UNREGULATED, ESU = ENZYMATIC_STANDARD_UNREGULATED,
    MICHAELIS_MENTEN_UNREGULATED, MMU = MICHAELIS_MENTEN_UNREGULATED,
    RECEPTOR_BINDING, RB = RECEPTOR_BINDING,
    CROSS_BOUNDARY_STANDARD_UNREGULATED, CBSU = CROSS_BOUNDARY_STANDARD_UNREGULATED,
    CROSS_BOUNDARY_ENZYMATIC_STANDARD_UNREGULATED, CBESU = CROSS_BOUNDARY_ENZYMATIC_STANDARD_UNREGULATED,
    CROSS_BOUNDARY_MICHAELIS_MENTEN_UNREGULATED, CBMMU = CROSS_BOUNDARY_MICHAELIS_MENTEN_UNREGULATED
};

extern std::unordered_map<REACTION_TYPE, std::string> reactionTypeToAcronym;

enum class COMPARTMENT_TYPE {
    NON_SPATIAL,
    CONTAINER
};

// Forward declaration to allow mutual referencing of classes.
class Molecule;
class Reaction;
class Compartment;
class FixedCountHandler;  // Not exposed to the interface. For implementation only.

class Molecule {
  public:
    /* For all constructors:
        *    newCompartment can't be NULL.
        *    newIndexInCompartment must be the index that this molecule will occupy in the molecules vector of newCompartment.
        * */
    Molecule(Compartment* newCompartment, const std::string& newName, int newIndexInCompartment);
    Molecule(Compartment* newCompartment, const std::string& newName, int newIndexInCompartment, double newInitialCount);
    ~Molecule();

    Compartment* getCompartment() const;
    // Returns index in the molecules vector of the containing compartment
    int getIndexInCompartment() const;

    const std::string& getName() const;

    // True if initial count was specified in L++ code through some assignment. False otherwise.
    bool hasInitialCount() const;
    // Throws error if molecule has no initial count. Check with hasInitialCount() first.
    double getInitialCount() const;
    void setInitialCount(double newInitialCount);

    /* See wiki/Compiler Context/Interface Notes/Molecule Indexing and Slicing/ for these three functions.
        *
        * The last two functions return a const map references. This means that the maps cannot be changed, which
        * means you cannot index them with [], as [] creates values and inserts them into the map if the key searched
        * for doesn't already exist. Use <map object>.at(key) instead, which throws an error if key is not in the map.
        * */
    std::optional<double> getBaseline() const;
    const std::map<double, double>& getChangePoints() const;
    const std::map<double, std::optional<double>>& getIntervalPoints() const;

  private:
    Compartment* const compartment;  // Cannot be NULL
    int indexInCompartment;

    const std::string name;
    std::optional<double> initialCount;

    // These are for implementation reasons.
    friend class FixedCountHandler;
    // If developing the implementation and need to access this, see FixedCountHandler declaration in context.cxx.
    FixedCountHandler* fixedCountHandler;
};

class Reaction {
  public:
    // newCompartment can't be NULL.
    Reaction(Compartment* newCompartment, const std::string& newName);
    Reaction(const Reaction& reaction);
    ~Reaction();

    Compartment* getCompartment() const;

    const std::string& getName() const;

    REACTION_TYPE getType() const;
    /* Considers whether the reaction can have type reactionType based on the parameters that it has.
        * This could return false for all possible reaction types, for example when a reaction has been
        * given contradictory parameters that don't match with any possible type.
        * */
    virtual bool canHaveType(REACTION_TYPE reactionType) const;
    void setType(REACTION_TYPE newType);

    const std::vector<Molecule*>& getReactants() const;
    void addReactant(Molecule* molecule, int stoichiometricCoefficient);

    const std::vector<Molecule*>& getProducts() const;
    void addProduct(Molecule* molecule, int stoichiometricCoefficient);

    bool hasProtein() const;
    Molecule* getProtein() const;
    void setProtein(Molecule* molecule);

    // Returns 0 for molecules that are not part of the reaction.
    int getStoichiometricCoefficient(Molecule* molecule);

    bool hasParameter(PARAM parameter) const;
    // Throws error if reaction does not have the parameter. Check with hasParameter() first.
    double getParameterValue(PARAM parameter) const;
    /* Returns a const map reference. This means that the map cannot be changed, which means you cannot index it
        * with [], as [] creates values and inserts them into the map if the key searched for doesn't already exist.
        * Use <map object>.at(key) instead, which throws an error if key is not in the map.
        * */
    const std::unordered_map<PARAM, double>& getParameters() const;
    void addParameter(PARAM parameterName, double value);

  private:
    Compartment* const compartment;  // Cannot be NULL
    const std::string name;
    REACTION_TYPE type;

    std::vector<Molecule*> reactants;
    std::vector<Molecule*> products;
    std::optional<Molecule*> protein;  // for ESU and MMU reactions

    std::unordered_map<Molecule*, int> stoichiometry;
    std::unordered_map<PARAM, double> parameters;
};

class Activation : public Reaction {
  public:
    /* The Activation() constructor is designed so that an 'old' reaction, that was previously thought to be of
        * some other type, can be passed in and converted to a new reaction. It copies over name, parameters, etc.
        * This process does not deallocate the old reaction, and assumes that the old reaction is well-formed.
        * */
    Activation(Reaction* oldReaction, const std::string& newActivationReactionName, Molecule* newActivator);
    ~Activation();

    // See wiki/Compiler Context/Interface Notes/Regular vs. Activation//Inhibition/
    const std::string& getActivationReactionName();

    Molecule* getActivator() const;

    // See wiki/Compiler Context/Interface Notes/Regular vs. Activation//Inhibition/ for the next 3 functions.
    bool hasActivationParameter(PARAM parameter) const;
    // Throws error if reaction does not have the parameter. Check with hasActivationParameter() first.
    double getActivationParameterValue(PARAM parameter) const;
    void addActivationParameter(PARAM parameterName, double value);

    /* Override which considers only activation parameters instead of normal parameters. Does not re-evaluate the
        * underlying reaction that was passed in to the constructor.
        * */
    virtual bool canHaveType(REACTION_TYPE reactionType) const override;

  private:
    const std::string activationReactionName;
    Molecule* const activator;
    std::unordered_map<PARAM, double> activationParameters;
};

class Inhibition : public Reaction {
  public:
    /* The Inhibition() constructor is designed so that an 'old' reaction, that was previously thought to be of
        * some other type, can be passed in and converted to a new reaction. It copies over name, parameters, etc.
        * This process does not deallocate the old reaction, and assumes that the old reaction is well-formed.
        * */
    Inhibition(Reaction* oldReaction, const std::string& newInhibitionReactionName, Molecule* newInhibitor);
    ~Inhibition();

    // See wiki/Compiler Context/Interface Notes/Regular vs. Activation//Inhibition/
    const std::string& getInhibitionReactionName();

    Molecule* getInhibitor() const;

    // See wiki/Compiler Context/Interface Notes/Regular vs. Activation//Inhibition/ for the next 3 functions.
    bool hasInhibitionParameter(PARAM parameter) const;
    // Throws error if reaction does not have the parameter. Check with hasInhibitionParameter() first.
    double getInhibitionParameterValue(PARAM parameter) const;
    void addInhibitionParameter(PARAM parameterName, double value);

    /* Override which considers only inhibition parameters instead of normal parameters. Does not re-evaluate the
        * underlying reaction that was passed in to the constructor.
        * */
    virtual bool canHaveType(REACTION_TYPE reactionType) const override;

  private:
    const std::string inhibitionReactionName;
    Molecule* const inhibitor;
    std::unordered_map<PARAM, double> inhibitionParameters;
};

// See wiki/Simulation Structure Overview/ and wiki/Compiler Context/ for more information.
class Compartment {
  public:
    // newParent can be NULL, but this should only be the case for the root container of the simulation.
    Compartment(Compartment* newParent, const std::string& newName, COMPARTMENT_TYPE newType);
    Compartment(Compartment* newParent, const std::string& newName, COMPARTMENT_TYPE newType, double newVolume);
    Compartment(Compartment* newParent, const std::string& newName, COMPARTMENT_TYPE newType, bool newIsSpatial);
    Compartment(Compartment* newParent, const std::string& newName, COMPARTMENT_TYPE newType, double newVolume, bool newIsSpatial);
    // Destructor deallocates all Compartments, Molecules and Reactions in the children, molecules and reactions vectors.
    ~Compartment();

    Compartment* getParent() const;

    const std::string& getName() const;

    COMPARTMENT_TYPE getType() const;

    double getVolume() const;

    bool isSpatialCompartment() const;

    const std::vector<Compartment*>& getChildren() const;
    void addChild(Compartment* child);

    const std::vector<Molecule*>& getMolecules() const;
    bool hasMolecule(const std::string& nameToSearch) const;
    // Throws error if compartment does not have the molecule. Check with hasMolecule() first.
    Molecule* getMolecule(const std::string& nameToFind) const;
    /* Adds molecule to the back of the molecules vector. This is important when constructing the molecule.
        * Find size with <compartment object>.getMolecules().size().
        * */
    void addMolecule(Molecule* molecule);
    /* Processes a molecule assignment, given a SymbolNode with symbol ASSIGNMENT from the AST that represents
        * a molecule assignment.
        *
        * Throws an error on logically invalid inputs (i.e. invalid inputs as a result of
        * syntactically correct but logically incorrect L++ code, such as a molecule assignment with a negative index).
        *
        * If inputs are not well-formed (i.e. AST nodes with children of unexpected types), then behavior is undefined.
        * See wiki/Compiler Context/Interface Notes/processMoleculeAssignments() Well-Formed Inputs/ for more info.
        * */
    void processMoleculeAssignment(SymbolNode* assignmentNode);
    // See wiki/Compiler Context/Interface Notes/Constant, Changed and Fixed Molecules (Interface)/ for these 3 variables.
    bool hasConstantMolecules = false;
    bool hasChangedMolecules = false;
    bool hasFixedMolecules = false;

    const std::vector<Reaction*>& getReactions() const;
    bool hasReaction(const std::string& nameToSearch) const;
    // Throws error if compartment does not have the reaction. Check with hasReaction() first.
    Reaction* getReaction(const std::string& nameToFind) const;
    void addReaction(Reaction* reaction);
    void removeReaction(Reaction* reaction);
    /* Processes a reaction, given a KeywordNode with keyword REACTION from the AST that represents a reaction.
        *
        * Throws an error on logically invalid inputs (i.e. invalid inputs as a result of
        * syntactically correct but logically incorrect L++ code, such as a reaction with contradictory parameters).
        *
        * If inputs are not well-formed (i.e. AST nodes with children of unexpected types), then behavior is undefined.
        *
        * Optionally takes a bool isInProtein and string proteinName, which means that this reaction was declared inside a protein,
        * and should be processed as such.
        * */
    void processReaction(KeywordNode* reactionNode, bool isInProtein = false, const std::string& proteinName = "NONAME");

    void processProtein(KeywordNode* proteinNode);

  private:
    Compartment* const parent;  // can be NULL
    const std::string name;
    const COMPARTMENT_TYPE type;
    double volume;
    bool isSpatial;

    std::vector<Compartment*> children;

    std::unordered_map<std::string, int> moleculeNameToIndex;
    std::vector<Molecule*> molecules;

    std::unordered_map<std::string, int> reactionNameToIndex;
    std::vector<Reaction*> reactions;

    void processReactants(ASTNode* equationLHS, Reaction* reaction);
    void processProducts(ASTNode* equationRHS, Reaction* reaction);
    bool checkForActivation(SymbolNode* rightArrowNode) const;
    void processActivation(const std::string& activationReactionName, Reaction* inProgressReaction, SymbolNode* equationAssignmentNode);
    void processInhibition(const std::string& inhibitionReactionName, Reaction* inProgressReaction, SymbolNode* equationAssignmentNode);
};

class Simulation {
  public:
    explicit Simulation(const std::string& newName);
    ~Simulation();

    const std::string& getName() const;

    Compartment* getGlobalCompartment() const;

    void buildContext(ASTNode* statement);
    void buildSimulation(ASTNode* tree);

  private:
    const std::string name;
    Compartment* const globalCompartment;
        
};

}