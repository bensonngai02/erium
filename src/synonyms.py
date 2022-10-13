# Program to extract data from .csv/.tsv files from the CheBI database
import pandas as pd
import sqlite3
import dask.dataframe as dd
from sqlalchemy import create_engine

def decompressMass():
    mass = pd.read_csv("./cid-mass", delim_whitespace=True, names=["ID", "Formula", "Monoisotopic Mass", "Exact Mass"])
    mass.drop("Monoisotopic Mass", axis=1, inplace=True)
    mass.drop("Exact Mass", axis=1, inplace=True)
    mass.to_csv("./cid-mass.csv", index=False)

def decompressSynonyms():
    synonyms = pd.read_csv("./cid-synonyms", delimiter="\t", names=["ID", "Synonym"])
    synonyms.to_csv("./cid-synonyms.csv", index=False)

def decompressCheBINames():
    names = pd.read_csv("./names_3star.tsv", delimiter="\t")
    names.drop(columns=["ID", "TYPE", "SOURCE", "ADAPTED", "LANGUAGE"], inplace=True)
    names.columns=["Compound ID", "Name"]
    names["Compound ID"] = names["Compound ID"].astype('int')
    names.sort_values(by="Compound ID", inplace=True)
    names.to_csv("./chebiNames.csv", index=False)

def decompressCheBIFormulas():
    formulas = pd.read_csv("https://ftp.ebi.ac.uk/pub/databases/chebi/Flat_file_tab_delimited/chemical_data.tsv", delimiter="\t")
    formulas.drop(columns=["ID", "SOURCE"], inplace=True)
    formulas.columns=["Compound ID", "Type", "Formula"]
    formulas.drop(formulas[formulas["Type"] != "FORMULA"].index, inplace=True)
    formulas.drop(columns=["Type"], inplace=True)
    formulas["Compound ID"] = formulas["Compound ID"].astype('int')
    formulas.sort_values(by="Compound ID", inplace=True)
    formulas.to_csv("./chebiFormulas.csv", index=False)

def decompressCheBICAS():
    cas = pd.read_csv("https://ftp.ebi.ac.uk/pub/databases/chebi/Flat_file_tab_delimited/database_accession.tsv", delimiter="\t")
    cas.drop(columns=["ID", "SOURCE"], inplace=True)
    cas.columns = ["Compound ID", "Type", "Accession Number"]
    cas.drop(cas.index[[cas["Type"] != "CAS Registry Number"]], inplace=True)
    cas.drop(columns=["Type"], inplace=True)
    cas["Compound ID"] = cas["Compound ID"].astype('int')
    cas.sort_values(by="Compound ID", inplace=True)
    cas.to_csv("./chebiCAS.csv", index=False)


def makeDatabase(input, output, outputName):
    data = pd.read_csv(input)
    chemicals = sqlite3.connect(output)
    data.to_sql(outputName, chemicals, if_exists='append', index=False)
    chemicals.close()

def filter():
    batch = pd.read_csv("./cid-synonyms01.csv")
    for index, row in batch.iterrows():
        if "CHEMBL" in row["Synonym"]:
            batch.drop(axis=0, index=index, inplace=True)
    batch.to_csv("./cid-synonyms01.csv")

def removeDuplicates(input):
    data = pd.read_csv(input)
    names = set()
    for index, row in data.iterrows():
        lowered = str(row["Name"]).lower()
        if lowered in names:
            data.drop(index=index, inplace=True)
        else:
            names.add(lowered);
    data.to_csv("./chebiChemicalsCASSet.csv", index=False)

def lowercaseNames(input):
    data = pd.read_csv(input)
    data["Name"] = data["Name"].str.upper()

    data.to_csv("./chebiChemicalsCASSetUPPER.csv", index=False)

def mergeCSVCheBI():
    formulaCSV = pd.read_csv("./chebiFormulas.csv")
    # add formulas
    formulas = {}
    for index, row in formulaCSV.iterrows(): 
        formulas[row["Compound ID"]] = row["Formula"]
    
    casCSV = dd.read_csv("./chebiCAS.csv");
    cas = {}
    for index, row in casCSV.iterrows():
        cas[row["Compound ID"]] = row["Accession Number"]

    formulaList = []
    casList = []
    missing = []

    synonyms = pd.read_csv("./chebiNames.csv")
    for index, row in synonyms.iterrows():
        # check the ID of the row
        # lookup the formula of that ID
        chosenFormula = formulas.get(row["Compound ID"])
        if chosenFormula:
            formulaList.append(chosenFormula)
        else:
            formulaList.append("MISSING")
            missing.append(row["Name"])

        chosenCAS = cas.get(row["Compound ID"])
        if chosenCAS:
            casList.append(chosenCAS)
        else:
            casList.append("MISSING")

    synonyms["Formula"] = formulaList;
    synonyms["CAS"] = casList;
    synonyms.to_csv("./chebiChemicalsCAS.csv", index=False)

        


# decompressMass()
# decompressSynonyms()
# filter()
# mergeCSV()

# decompressCheBINames()
# decompressCheBIFormulas()
# decompressCheBICAS()
# mergeCSVCheBI()
# removeDuplicates("./chebiChemicalsCAS.csv")
# makeDatabase("./chebiChemicals.csv", "./chemBIChemicals.db", "chemBIChemicals")
# makeDatabase("./chebiChemicalsCAS.csv", "./chemBIChemicalsCAS.db", "chemBIChemicalsCAS")
lowercaseNames("chebiChemicalsCASSet.csv")
makeDatabase("./chebiChemicalsCASSetUpper.csv", "./chemBIChemicalsCASSetUpper.db", "chemBIChemicalsCASSetUpper")