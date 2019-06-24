#ifndef GENPARTANALYZER_H
#define GENPARTANALYZER_H

#include <ChargedHiggs/NanoSkimming/interface/baseanalyzer.h>

//Jet class to be safed in tree
struct GenPart {
    TLorentzVector Hc;
    TLorentzVector h1;
    TLorentzVector h2;
    TLorentzVector W;
};


class GenPartAnalyzer: public BaseAnalyzer{
    private:
        //Bool for checking if data file
        bool isData;

        //Gen particle vector
        GenPart genParts;

    public:
        GenPartAnalyzer();
        void BeginJob(TTreeReader &reader, TTree* tree, bool &isData);
        bool Analyze(std::pair<TH1F*, float> &cutflow);
        void EndJob(TFile* file);
};

#endif