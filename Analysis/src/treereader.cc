#include <ChargedAnalysis/Analysis/include/treereader.h>

//Constructor

TreeReader::TreeReader(){}

TreeReader::TreeReader(const std::vector<std::string> &parameters, const std::vector<std::string> &cutStrings, const std::string &outname, const std::string &channel):
    parameters(parameters),
    cutStrings(cutStrings),
    outname(outname),
    channel(channel){
}

void TreeReader::GetFunction(const std::string& parameter, Function& func, FuncArgs& args){
    if(Utils::Find<std::string>(parameter, "f:") == -1) throw std::runtime_error("No function key 'f' in '" + parameter + "'");

    std::string funcLine = parameter.substr(parameter.find("f:")+2, parameter.substr(parameter.find("f:")).find("/")-2);

    for(std::string& funcParam: Utils::SplitString<std::string>(funcLine, ",")){
        std::vector<std::string> fInfo = Utils::SplitString<std::string>(funcParam, "=");

        if(fInfo[0] == "n") func.func = TreeFunction::functions[fInfo[1]];
        else if(fInfo[0] == "v") args.value = std::stoi(fInfo[1]);
        else throw std::runtime_error("Invalid key '" + fInfo[0] + "' in parameter '" +  funcLine + "'");
    }
}

void TreeReader::GetParticle(const std::string& parameter, FuncArgs& args){
    if(Utils::Find<std::string>(parameter, "p:") == -1) return;

    std::string partLine = parameter.substr(parameter.find("p:")+2, parameter.substr(parameter.find("p:")).find("/")-2);

    for(std::string& particle: Utils::SplitString<std::string>(partLine, "~")){
        for(const std::string& partParam: Utils::SplitString<std::string>(particle, ",")){
            std::vector<std::string> pInfo = Utils::SplitString<std::string>(partParam, "=");

            if(pInfo[0] == "n") args.parts.push_back(TreeFunction::particles[pInfo[1]]);
            else if (pInfo[0] == "wp") args.wp.push_back(TreeFunction::workingPoints[pInfo[1]]);
            else if (pInfo[0] == "i") args.index.push_back(std::atoi(pInfo[1].c_str()) - 1);
            else throw std::runtime_error("Invalid key '" + pInfo[0] + "' in parameter '" +  partLine + "'");
        }

        if(args.parts.size() > args.wp.size()) args.wp.push_back(NONE);
        if(args.parts.size() > args.index.size()) args.index.push_back(0);
    }
}

void TreeReader::GetCut(const std::string& parameter, FuncArgs& args){
    if(Utils::Find<std::string>(parameter, "c:") == -1) throw std::runtime_error("No cut key 'c' in '" + parameter + "'");

    std::string cutLine = parameter.substr(parameter.find("c:")+2, parameter.substr(parameter.find("c:")).find("/")-2);

    for(std::string& cutParam: Utils::SplitString<std::string>(cutLine, ",")){
        std::vector<std::string> cInfo = Utils::SplitString<std::string>(cutParam, "=");

        if(cInfo[0] == "n") args.comp = TreeFunction::comparisons[cInfo[1]];
        else if(cInfo[0] == "v") args.compValue = std::stof(cInfo[1]);
        else throw std::runtime_error("Invalid key '" + cInfo[0] + "' in parameter '" +  cutLine + "'");
    }
}

void TreeReader::GetBinning(const std::string& parameter, TH1* hist){
    if(Utils::Find<std::string>(parameter, "h:") == -1) throw std::runtime_error("No hist key 'h' in '" + parameter + "'");

    std::string histLine = parameter.substr(parameter.find("h:")+2, parameter.substr(parameter.find("h:")).find("/")-2);

    int bins = 30; float xlow = 0; float xhigh = 1; 

    for(std::string& histParam: Utils::SplitString<std::string>(histLine, ",")){
        std::vector<std::string> hInfo = Utils::SplitString<std::string>(histParam, "=");

        if(hInfo[0] == "nxb") bins = std::stof(hInfo[1]);
        else if(hInfo[0] == "xl") xlow = std::stof(hInfo[1]);
        else if(hInfo[0] == "xh") xhigh = std::stof(hInfo[1]);
        else throw std::runtime_error("Invalid key '" + hInfo[0] + " in parameter '" +  histLine + "'");
    }

    hist->SetBins(bins, xlow, xhigh);
}

void TreeReader::PrepareLoop(TFile* outFile){
    for(const std::string& parameter: parameters){
        //Functor structure and arguments
        Function function; FuncArgs arg;
        
        GetFunction(parameter, function, arg);
        GetParticle(parameter, arg);

        //Set axis label name
        std::string name = TreeFunction::functions[function.func] + (arg.value != -999. ? std::to_string(arg.value) : "");
        std::string xLabel = TreeFunction::funcLabels[function.func];

        if(arg.value != -999.){
            xLabel = Utils::Format<int>("@", xLabel, arg.value, true);
        }

        for(int i=0; i<arg.parts.size(); i++){
            std::string partLabel = Utils::Format<std::string>("@", TreeFunction::partLabels[arg.parts[i]], function.func == &TreeFunction::NParticle ? "" : std::to_string(arg.index[i]+1), arg.parts[i] == MET ? true : false);
            xLabel = Utils::Format<std::string>("@", xLabel, partLabel);

            name += "_" + TreeFunction::particles[arg.parts[i]] + (arg.parts[i] == MET ? "" : std::to_string(arg.index[i]+1)) + (arg.wp[i] == NONE ? "" : TreeFunction::workingPoints[arg.wp[i]]);
        }

        if(Utils::Find<std::string>(parameter, "h:") != -1){
            TH1F* hist = new TH1F();

            TreeReader::GetBinning(parameter, hist);

            hist->SetName(name.c_str());       
            hist->SetTitle(name.c_str());
            hist->SetDirectory(outFile);

            hist->GetXaxis()->SetTitle(xLabel.c_str());              

            hists.push_back(hist);
            histArgs.push_back(arg);
            histFunctions.push_back(function);
        }

        if(Utils::Find<std::string>(parameter, "t:") != -1){
            if(outTree == NULL){
                outTree = new TTree(channel.c_str(), channel.c_str());
                outTree->SetDirectory(outFile);
            }

            branchNames.push_back(name);
            treeValues.push_back(1.);

            treeArgs.push_back(arg);
            treeFunctions.push_back(function);
        }

        if(Utils::Find<std::string>(parameter, "csv:") != -1){
            CSVNames.push_back(name);

            CSVArgs.push_back(arg);
            CSVFunctions.push_back(function);
        }
    }

    //Declare branches of output tree if wished
    for(int i=0; i < branchNames.size(); i++){
        outTree->Branch(branchNames[i].c_str(), &treeValues[i]);
    }

    //Declare columns in CSV file if wished
    if(!CSVNames.empty()){
        frame = new Frame();
        frame->InitLabels(CSVNames);
    }

    for(const std::string& cut: cutStrings){
        //Functor structure and arguments
        Function function; FuncArgs arg;

        GetFunction(cut, function, arg);
        GetParticle(cut, arg);
        GetCut(cut, arg);

        //Set axis label name
        std::string name = TreeFunction::functions[function.func] + (arg.value != -999. ? std::to_string(arg.value) : "");
        std::string xLabel = TreeFunction::funcLabels[function.func];

        if(arg.value != -999.){
            xLabel = Utils::Format<int>("@", xLabel, arg.value, true);
        }

        for(int i=0; i<arg.parts.size(); i++){
            std::string partLabel = Utils::Format<std::string>("@", TreeFunction::partLabels[arg.parts[i]], function.func == &TreeFunction::NParticle ? "" : std::to_string(arg.index[i]+1), arg.parts[i] == MET ? true : false);
            xLabel = Utils::Format<std::string>("@", xLabel, partLabel);
        }

        std::string compV = std::to_string(arg.compValue);
        compV.erase(compV.find_last_not_of('0') + 1, std::string::npos); 
        compV.erase(compV.find_last_not_of('.') + 1, std::string::npos);
        std::map<Comparison, std::string> compStr = {{BIGGER, ">"}, {SMALLER, "<"}, {EQUAL, "=="}};
        cutLabels.push_back(xLabel + " " + compStr[arg.comp] + " " + compV);

        cutArgs.push_back(arg);
        cutFunctions.push_back(function);

        std::cout << "Cut will be applied: '" << cutLabels.back() << "'" << std::endl;
    }
}

template <typename TreeObject>
void TreeReader::PrepareEvent(TreeObject inputTree){
    //Name of TBranch mapped to objects holding the read out branch values
    std::map<std::string, std::map<Particle, std::vector<float>*>&> floatProperties = {
        {"Px", Px},
        {"Py", Py},
        {"Pz", Pz},
        {"E", E},
        {"FatJetIdx", FatJetIdx},
        {"Njettiness1", oneSubJettiness},
        {"Njettiness2", twoSubJettiness},
        {"Njettiness3", threeSubJettiness},
        {"looseSF", looseSF},
        {"mediumSF", mediumSF},
        {"tightSF", tightSF},
        {"triggerSF", triggerSF},
        {"recoSF", recoSF},
        {"loosebTagSF", loosebTagSF},
        {"mediumbTagSF", mediumbTagSF},
        {"tightbTagSF", tightbTagSF},
        {"looseIsoLooseSF", looseIsoLooseSF},
        {"looseIsoMediumSF", looseIsoMediumSF},
        {"looseIsoTightSF", looseIsoTightSF},
        {"tightIsoMediumSF", tightIsoMediumSF},
        {"tightIsoTightSF", tightIsoTightSF},
    };

    std::map<std::string, std::map<Particle, std::vector<bool>*>&> boolProperties = {
        {"isLoose", isLoose},
        {"isMedium", isMedium},
        {"isTight", isTight},
        {"isLooseB", isLooseB},
        {"isMediumB", isMediumB},
        {"isTightB", isTightB},
        {"isLooseIso", isLooseIso},
        {"isMediumIso", isMediumIso},
        {"isTightIso", isTightIso},
    };

    std::map<std::string, Particle> particles = {
        {"Electron", ELECTRON},
        {"Muon", MUON},
        {"Jet", JET},
        {"FatJet", FATJET},
    };

    //Set all branch address for all particles/quantities
    for(std::pair<const std::string, Particle>& part: particles){
        for(std::pair<const std::string, std::map<Particle, std::vector<float>*>&> property: floatProperties){
            std::string branchName = Utils::Join("_", {part.first, property.first});

            if(inputTree->GetListOfBranches()->Contains(branchName.c_str())){
                property.second[part.second] = NULL;
                inputTree->SetBranchAddress(branchName.c_str(), &property.second[part.second]);
            }
        }

        for(std::pair<const std::string, std::map<Particle, std::vector<bool>*>&> property: boolProperties){
            std::string branchName = Utils::Join("_", {part.first, property.first});

            if(inputTree->GetListOfBranches()->Contains(branchName.c_str())){
                property.second[part.second] = NULL;
                inputTree->SetBranchAddress(branchName.c_str(), &property.second[part.second]);
            }
        }
    }

    //Vector of  weights
    std::vector<std::string> weightNames = {"lumi", "xsec", "prefireWeight"};
    weights = std::vector<float>(weightNames.size(), 0.);

    for(unsigned int idx=0; idx < weightNames.size(); idx++){;
        inputTree->SetBranchAddress(("Weight_" + weightNames[idx]).c_str(), &weights[idx]);
    }

    //Set all other branch values needed
    inputTree->SetBranchAddress("MET_Px", &MET_Px); 
    inputTree->SetBranchAddress("MET_Py", &MET_Py);
    inputTree->SetBranchAddress("Misc_eventNumber", &eventNumber);
    inputTree->SetBranchAddress("Misc_TrueInteraction", &nTrue);

    //BDT score branches
    for(int i=0; i < inputTree->GetListOfBranches()->GetSize(); i++){
        std::string branchName = inputTree->GetListOfBranches()->At(i)->GetName();

        if(Utils::Find<std::string>(branchName, "BDT") != -1.){
            int mass = std::stoi(branchName.substr(branchName.size()-3,3));
            bdtScore[mass] = -999.;
            inputTree->SetBranchAddress(branchName.c_str(), &bdtScore[mass]);
        }

        if(Utils::Find<std::string>(branchName, "DNN") != -1.){
            int mass = std::stoi(branchName.substr(branchName.size()-3,3));
            dnnScore[mass] = -999.;
            inputTree->SetBranchAddress(branchName.c_str(), &dnnScore[mass]);
        }
    }
}

template void TreeReader::PrepareEvent(TTree* inputTree);
template void TreeReader::PrepareEvent(TChain* inputTree);

void TreeReader::SetEvent(Event& event, const bool& isData, const Particle& cleanPart, const WP& cleanWP){
    //Clear event
    event.particles.clear();
    event.SF.clear();
    event.subtiness.clear();
    event.eventNumber = eventNumber;
    event.bdtScore = bdtScore;
    event.dnnScore = dnnScore;

    //Get all particles and put them into event container by particle type/working point
    for(const Particle part: {ELECTRON, MUON, JET, FATJET}){
        for(int j=0; j < Px[part]->size(); j++){
            ROOT::Math::PxPyPzEVector LV(Px[part]->at(j), Py[part]->at(j), Pz[part]->at(j), E[part]->at(j));
            if(part==ELECTRON){
                if(isTight[part]->at(j) && isTightIso[part]->at(j) && LV.Pt() > 30){
                    event.particles[part][TIGHT].push_back(LV);
                    event.SF[part][TIGHT].push_back(isData ? 1 : tightSF[part]->at(j) * Utils::CheckZero(recoSF[part]->at(j)));
                }

                if(isMedium[part]->at(j) && isMediumIso[part]->at(j) && LV.Pt() > 30){
                    event.particles[part][MEDIUM].push_back(LV);
                    event.SF[part][MEDIUM].push_back(isData ? 1 : mediumSF[part]->at(j)*Utils::CheckZero(recoSF[part]->at(j)));
                }

                if(isLoose[part]->at(j) && isLooseIso[part]->at(j) && LV.Pt() > 30){
                    event.particles[part][LOOSE].push_back(LV);
                    event.SF[part][LOOSE].push_back(isData ? 1 : looseSF[part]->at(j)*Utils::CheckZero(recoSF[part]->at(j)));
                }

                event.particles[part][NONE].push_back(LV);
            }

            if(part==MUON){
                if(isTight[part]->at(j) && isTightIso[part]->at(j) && LV.Pt() > 30){
                    event.particles[part][TIGHT].push_back(LV);
                    event.SF[part][TIGHT].push_back(isData ? 1 : Utils::CheckZero(tightSF[part]->at(j))*Utils::CheckZero(looseIsoTightSF[part]->at(j))*Utils::CheckZero(triggerSF[part]->at(j)));
                }

                if(isMedium[part]->at(j) && isTightIso[part]->at(j) && LV.Pt() > 30){
                    event.particles[part][MEDIUM].push_back(LV);
                    event.SF[part][MEDIUM].push_back(isData ? 1 : Utils::CheckZero(mediumSF[part]->at(j))*Utils::CheckZero(tightIsoMediumSF[part]->at(j))*Utils::CheckZero(triggerSF[part]->at(j)));
                }

                if(isLoose[part]->at(j) && isTightIso[part]->at(j) && LV.Pt() > 30){
                    event.particles[part][LOOSE].push_back(LV);
                    event.SF[part][LOOSE].push_back(isData ? 1 : Utils::CheckZero(looseSF[part]->at(j))*Utils::CheckZero(tightIsoTightSF[part]->at(j))*Utils::CheckZero(triggerSF[part]->at(j)));
                }
                   
                event.particles[part][NONE].push_back(LV);
            }

            if(part==JET){
                bool isCleaned=true;

                if(cleanPart != NOTHING){
                    for(ROOT::Math::PxPyPzEVector& p: event.particles[cleanPart][cleanWP]){
                        if(ROOT::Math::VectorUtil::DeltaR(p, LV) < 0.4){
                            isCleaned=false;
                            break;
                        }
                    }
                }

                bool isSubJet=false;
                if(FatJetIdx[part]->size() != 0){
                    if(FatJetIdx[part]->at(j) != -1) isSubJet=true; 
                }

                if(!isCleaned and !isSubJet) continue;

                if(isTightB[part]->at(j)){
                    if(isSubJet){
                        event.particles[BSUBJET][TIGHT].push_back(LV);
                        event.SF[BSUBJET][TIGHT].push_back(isData ? 1 : tightbTagSF[part]->at(j));
                    }

                    else{
                        event.particles[BJET][TIGHT].push_back(LV);
                        event.SF[BJET][TIGHT].push_back(isData ? 1 : tightbTagSF[part]->at(j));
                    }
                }

                if(isMediumB[part]->at(j)){
                    if(isSubJet){
                        event.particles[BSUBJET][MEDIUM].push_back(LV);
                        event.SF[BSUBJET][MEDIUM].push_back(isData ? 1 : mediumbTagSF[part]->at(j));
                    }

                    else{
                        event.particles[BJET][MEDIUM].push_back(LV);
                        event.SF[BJET][MEDIUM].push_back(isData ? 1 : mediumbTagSF[part]->at(j));
                    }
                }

                if(isLooseB[part]->at(j)){
                    if(isSubJet){
                        event.particles[BSUBJET][LOOSE].push_back(LV);
                        event.SF[BSUBJET][LOOSE].push_back(isData ? 1 : loosebTagSF[part]->at(j));
                    }

                    else{
                        event.particles[BJET][LOOSE].push_back(LV);
                        event.SF[BJET][LOOSE].push_back(isData ? 1 : loosebTagSF[part]->at(j));
                    }
                }

                if(isSubJet) event.particles[SUBJET][NONE].push_back(LV);
                else event.particles[JET][NONE].push_back(LV);
            }

            if(part==FATJET){
                event.particles[FATJET][NONE].push_back(LV);
                event.subtiness.push_back({oneSubJettiness[part]->at(j), twoSubJettiness[part]->at(j), threeSubJettiness[part]->at(j)});
            }
        }
    }

    //MET
    event.particles[MET][NONE].push_back(ROOT::Math::PxPyPzEVector(MET_Px, MET_Py, 0, 0));
}

void TreeReader::EventLoop(const std::string &fileName, const int &entryStart, const int &entryEnd, const std::string& cleanJet){
    //Get input tree
    TFile* inputFile = TFile::Open(fileName.c_str(), "READ");
    TTree* inputTree = (TTree*)inputFile->Get(channel.c_str());

    std::cout << "Read file: '" << fileName << "'" << std::endl;
    std::cout << "Read tree '" << channel << "' from " << entryStart << " to " << entryEnd << std::endl;

    TH1::AddDirectory(kFALSE);
    gROOT->SetBatch(kTRUE);

    std::string name = outname;
    if(name.find("csv") != std::string::npos) name.replace(name.find("csv"), 3, "root");

    //Open output file and set up all histograms/tree and their function to call
    TFile* outFile = TFile::Open(name.c_str(), "RECREATE");
    PrepareLoop(outFile);

    //Determine what to clean from jets
    std::vector<std::string> cleanInfo = Utils::SplitString<std::string>(cleanJet, "/");
    Particle partToClean; WP wpToClean;

    if(cleanInfo.size() != 1){
        partToClean=TreeFunction::particles[cleanInfo[0]];
        wpToClean=TreeFunction::workingPoints[cleanInfo[1]];
    }

    //Get number of generated events
    if(inputFile->GetListOfKeys()->Contains("nGen")){
        nGen = ((TH1F*)inputFile->Get("nGen"))->Integral();
    }

    //Calculate pile up weight histogram
    TH1F* pileUpWeight=NULL;
    bool isData=true;

    if(inputFile->GetListOfKeys()->Contains("puMC")){
        TH1F* puMC=NULL; TH1F* puReal=NULL;
        isData=false;

        puMC = (TH1F*)inputFile->Get("puMC");
        puReal = (TH1F*)inputFile->Get("pileUp");

        pileUpWeight = (TH1F*)puReal->Clone();
        pileUpWeight->Scale(1./pileUpWeight->Integral());
        puMC->Scale(1./puMC->Integral());

        pileUpWeight->Divide(puMC);
        delete puMC; delete puReal;
    }

    //Cutflow
    TH1F* cutflow = (TH1F*)inputFile->Get(("cutflow_" + channel).c_str())->Clone();
    cutflow->SetName("cutflow"); cutflow->SetTitle("cutflow");
    if(inputTree->GetEntries() != 0) cutflow->Scale((1./nGen)*(entryEnd-entryStart)/inputTree->GetEntries());
    else cutflow->Scale(1./nGen);

    //Set all branch addresses needed for the event
    PrepareEvent<TTree*>(inputTree);
    Event event;

    for (int i = entryStart; i < entryEnd; i++){
        //Load event
        inputTree->GetEntry(i);

        //Fill event class with particle content
        SetEvent(event, isData, partToClean, wpToClean);

        //Multiply all common weights
        float weight = 1.;

        for(float& w: weights){
            weight *= w;
        }

        weight*= 1./nGen;
        if(pileUpWeight!=NULL) weight *= pileUpWeight->GetBinContent(pileUpWeight->FindBin(nTrue));

        //Check if event passed all cuts
        bool passed=true;

        for(int j=0; j < cutFunctions.size(); j++){
            event.weight = 1.;
            passed = passed && cutFunctions[j](event, cutArgs[j], true);
            weight *= event.weight;

            if(passed){
                cutflow->Fill(cutLabels[j].c_str(), weight);
            }

            else break;
        }

        if(!passed) continue;

        //Fill histogram
        for(int j=0; j < hists.size(); j++){
            event.weight = 1.;
            float value = histFunctions[j](event, histArgs[j]);

            hists[j]->Fill(value, weight*event.weight);
        }

        //Fill trees
        for(int j=0; j < treeFunctions.size(); j++){
            treeValues[j] = treeFunctions[j](event, treeArgs[j]);
        }

        if(outTree != NULL) outTree->Fill();

        //Fill CSV file
        if(frame != NULL){
            std::vector<float> values;

            for(int j=0; j < CSVFunctions.size(); j++){
                values.push_back(CSVFunctions[j](event, CSVArgs[j]));
            }

            frame->AddColumn(values);
        }
    }

    //Write all histograms and delete everything
    outFile->cd();
    for(TH1F* hist: hists){
        hist->Write(); 
        std::cout << "Saved histogram: '" << hist->GetName() << "' with " << hist->GetEntries() << " entries" << std::endl;
        delete hist;
    }

    if(outTree != NULL){
        outTree->Write(); 
        std::cout << "Saved tree: '" << outTree->GetName() << "' with " << outTree->GetEntries() << " entries" << std::endl;
        std::cout << "Branches which are saved in tree:" << std::endl;

        for(std::string& branchName: branchNames){
            std::cout << branchName << std::endl;
        }

        delete outTree;
    }

    //Remove empty bins and write cutflow
    int nonEmpty = 0;

    for(int i=0; i < cutflow->GetNbinsX(); i++){
        if(std::string(cutflow->GetXaxis()->GetBinLabel(i)) != "") nonEmpty++;
    }

    cutflow->SetAxisRange(0, nonEmpty); 
    cutflow->Write();

    if(frame!=NULL){
        frame->WriteCSV(outname);
        delete frame;
    }

    //Delete everything
    delete cutflow; delete inputTree; delete inputFile; delete outFile;

    std::cout << "Closed output file: '" << outname << "'" << std::endl;
}
