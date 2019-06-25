#include <ChargedHiggs/Analysis/interface/treereader.h>

//Constructor

TreeReader::TreeReader(){}

TreeReader::TreeReader(std::string &process, std::vector<std::string> &xParameters, std::vector<std::string> &yParameters, std::vector<std::string> &cutStrings, std::string &outname, std::string &channel, const bool& saveTree):
    process(process),
    xParameters(xParameters),
    yParameters(yParameters),
    cutStrings(cutStrings),
    outname(outname),
    channel(channel),
    saveTree(saveTree){

    //Start measure execution time
    start = std::chrono::steady_clock::now();

    //Maps of all strings/enumeration
    strToOp = {{">", BIGGER}, {">=", EQBIGGER}, {"==", EQUAL}, {"<=", EQSMALLER}, {"<", SMALLER},  {"%", DIVISIBLE}, {"%!", NOTDIVISIBLE}};
    strToPart = {{"e", ELECTRON}, {"mu", MUON}, {"j", JET}, {"sj", SUBJET}, {"bsj", BSUBJET}, {"bj", BJET}, {"fj", FATJET}, {"bfj", BFATJET}, {"met", MET}, {"W", W}, {"Hc", HC}, {"h", h}, {"t", TOP}};
    partLabel = {{ELECTRON, "e_{@}"}, {MUON, "#mu_{@}"}, {JET, "j_{@}"}, {SUBJET, "j^{sub}_{@}"}, {FATJET, "j_{@}^{AK8}"}, {BJET, "b-tagged j_{@}"}, {MET, "#slash{E}_{T}"}, {W, "W^{#pm}"}, {HC, "H^{#pm}"}, {h, "h_{@}"}, {TOP, "t_{@}"}};
    nPartLabel = {{ELECTRON, "electrons"}, {MUON, "muons"}, {JET, "jets"}, {FATJET, "fat jets"}, {BJET, "b-tagged jets"}, {BFATJET, "b-tagged fat jets"}, {SUBJET, "sub jets"}, {BSUBJET, "b-tagged sub jets"}};

    strToFunc = {
                    {"m", MASS}, 
                    {"phi", PHI}, 
                    {"eta", ETA}, 
                    {"pt", PT},     
                    {"dphi", DPHI},
                    {"dR", DR},
                    {"N", NPART},
                    {"HT", HT},
                    {"evNr", EVENTNUMBER},
                    {"bdt", BDTSCORE},
                    {"const", CONSTNUM},
                    {"Nsig", NSIGPART},
                    {"tau", SUBTINESS},
                    {"phistar", PHISTAR},
    };
    
    funcLabel = {
                    {MASS, "m(@) [GeV]"}, 
                    {PHI, "#phi(@) [rad]"}, 
                    {ETA, "#eta(@) [rad]"}, 
                    {PT, "p_{T}(@) [GeV]"}, 
                    {DPHI, "#Delta#phi(@, @) [rad]"}, 
                    {DR, "#DeltaR(@, @) [rad]"},
                    {HT, "H_{T} [GeV]"}, 
                    {BDTSCORE, "BDT score"},
                    {NPART, "N_{@}"},
                    {CONSTNUM, "Bin number"},
                    {NSIGPART, "N^{gen matched}_{@}"},
                    {SUBTINESS, "#tau(@)"},
                    {PHISTAR, "#phi^{*}"},
    };

    //Maps of all binning, functions and SF
    binning = {
                {MASS, {20., 100., 600.}},
                {PT, {30., 0., 200.}},
                {ETA, {30., -2.4, 2.4}},
                {PHI, {30., -TMath::Pi(), TMath::Pi()}},
                {DPHI, {30., 0., TMath::Pi()}},
                {DR, {30., 0., 6.}},
                {HT, {30., 0., 500.}},
                {NPART, {6., 0., 6.}},
                {BDTSCORE, {30., -0.5, 0.3}},
                {CONSTNUM, {3., 0., 2.}},
                {NSIGPART, {5., 0., 5.}},
                {SUBTINESS, {30., 0., 0.4}},
                {PHISTAR, {50., 0., TMath::Pi()}},
    };

    funcDir = {
                {MASS, &TreeReader::Mass},
                {PT, &TreeReader::Pt},
                {PHI, &TreeReader::Phi},
                {ETA, &TreeReader::Eta},
                {DPHI, &TreeReader::DeltaPhi},
                {DR, &TreeReader::DeltaR},
                {HT, &TreeReader::HadronicEnergy},
                {NPART, &TreeReader::NParticle},
                {EVENTNUMBER, &TreeReader::EventNumber},
                {BDTSCORE, &TreeReader::BDTScore},
                {CONSTNUM, &TreeReader::ConstantNumber},
                {NSIGPART, &TreeReader::NSigParticle},
                {SUBTINESS, &TreeReader::Subtiness},
                {PHISTAR, &TreeReader::PhiStar},
    };

    eleID = {{1, {&Electron::isMedium, 0.2}}, {1, {&Electron::isTight, 0.1}}};
    eleSF = {{1, &Electron::mediumMvaSF}, {2, &Electron::mediumMvaSF}};

    muonID = {{1, {&Muon::isMedium, &Muon::isLooseIso}}, {2, {&Muon::isTight, &Muon::isTightIso}}};
    muonSF = {{1, {&Muon::mediumSF, &Muon::looseIsoMediumSF}}, {2, {&Muon::tightSF, &Muon::tightIsoTightSF}}};

    bJetID = {{0, &Jet::isLooseB}, {1, &Jet::isMediumB}, {2, &Jet::isTightB}};
    bJetSF = {{0, &Jet::loosebTagSF}, {1, &Jet::mediumbTagSF}, {2, &Jet::tightbTagSF}};

    //Cut configuration
    for(std::string &cut: cutStrings){
        cuts.push_back(ConvertStringToEnums(cut, true));
    }

    //Check if cutflow should be written out
    std::vector<std::string>::iterator it = std::find(this->xParameters.begin(), this->xParameters.end(), "cutflow");

    if(it != this->xParameters.end()){
        writeCutFlow = true;
        this->xParameters.erase(it);
    }   
}

void TreeReader::ProgressBar(const int &progress){
    std::string progressBar = "["; 

    for(int i = 0; i < progress; i++){
        if(i%2) progressBar += "#";
    }

    for(int i = 0; i < 100 - progress; i++){
        if(i%2) progressBar += " ";
    }

    progressBar = progressBar + "] " + "Progress of process " + process + ": " + std::to_string(progress) + "%";
    std::cout << "\r" << progressBar << std::flush;

    if(progress == 100) std::cout << std::endl;

}

TreeReader::Hist TreeReader::ConvertStringToEnums(std::string &input, const bool &isCutString){
    //Function which handles splitting of string input
    std::vector<std::string> splittedString;
    std::string string;
    std::istringstream splittedStream(input);
    while (std::getline(splittedStream, string,  '_')){
        splittedString.push_back(string);
    }

    //Translate strings into enumeration
    Function func = strToFunc[splittedString[0]];
    float funcValue = -999.;
    Operator op; float cutValue;
    std::vector<Particle> parts;
    std::vector<int> indeces;

    //Check if variable is BDT
    if(func == BDTSCORE) isBDT = true;

    //If functions has special value
    int partStart = 1;
    int partEnd = isCutString ? 2 : 0;

    if(splittedString.size() > 1){
        try{
            funcValue = std::stof(splittedString[1]);
            partStart++;
        }

        catch(...){}
    }

    //If no particle is involved, like HT_>_100
    if(splittedString.size() == 3 and isCutString){
        op = strToOp[splittedString[1]];
        cutValue = std::stof(splittedString[2]);
    }
    
    else{
        for(std::string part: std::vector<std::string>(splittedString.begin()+partStart, splittedString.end() - partEnd)){ 
            try{
                indeces.push_back(std::stoi(std::string(part.end()-1 ,part.end())));
                parts.push_back(strToPart[std::string(part.begin(),part.end()-1)]);
            }

            catch(...){
                indeces.push_back(-1);
                parts.push_back(strToPart[part]);
            }
        }

        if(isCutString){
            std::vector<std::string> cutVec(splittedString.end() - 2, splittedString.end());

            std::string fLabel = funcLabel[func];

            for(unsigned int k = 0; k < parts.size(); k++){
                std::string pLabel = (func != NPART and func != NSIGPART) ? partLabel[parts[k]] : nPartLabel[parts[k]];

                if(pLabel.find("@") != std::string::npos){
                    pLabel.replace(pLabel.find("@"), 1, std::to_string(indeces[k]));
                }

                if(fLabel.find("@") != std::string::npos){
                    fLabel.replace(fLabel.find("@"), 1, pLabel);
                }
            }

            std::string cutName = fLabel + " " + cutVec[0] + " " + cutVec[1];
            cutNames.push_back(cutName);

            op = strToOp[cutVec[0]];
            cutValue = std::stof(cutVec[1]);
        }
    }

    return {NULL, NULL, parts, indeces, func, funcValue, {op, cutValue}};
}

std::tuple<std::vector<TreeReader::Hist>, std::vector<std::vector<TreeReader::Hist>>> TreeReader::SetHistograms(TFile* outputFile){
    //Histograms
    std::vector<Hist> histograms1D;
    std::vector<std::vector<Hist>> histograms2D;

    //Save pairs of XY parameters to avoid redundant plots
    std::vector<std::string> parameterPairs;

    //Define final histogram for each parameter
    for(unsigned int i = 0; i < xParameters.size(); i++){
        //Vector for 2D hists
        std::vector<Hist> temp2DHist;

        //Split input string into information for particle and function to call value
        Hist confX = ConvertStringToEnums(xParameters[i]);

        //Create final histogram
        TH1F* hist1D = new TH1F(xParameters[i].c_str(), xParameters[i].c_str(), binning[confX.func][0], binning[confX.func][1], binning[confX.func][2]);
        hist1D->Sumw2();
        hist1D->SetDirectory(outputFile);

        std::string fLabelX = funcLabel[confX.func];
        
        for(unsigned int k = 0; k < confX.parts.size(); k++){
            std::string pLabel = (confX.func != NPART and confX.func != NSIGPART) ? partLabel[confX.parts[k]] : nPartLabel[confX.parts[k]];

            if(pLabel.find("@") != std::string::npos){
                pLabel.replace(pLabel.find("@"), 1, std::to_string(confX.indeces[k]));
            }

            if(fLabelX.find("@") != std::string::npos){
                fLabelX.replace(fLabelX.find("@"), 1, pLabel);
            }
        }

        hist1D->GetXaxis()->SetTitle(fLabelX.c_str());
        confX.hist1D = hist1D;

        //Add hist to collection
        histograms1D.push_back(confX);

        for(unsigned int j = 0; j < yParameters.size(); j++){
            //Split input string into information for particle and function to call value
            Hist confY = ConvertStringToEnums(yParameters[j]);

            bool isNotRedundant = true;

            for(std::string pair: parameterPairs){
                if(pair.find(xParameters[i]) == std::string::npos and pair.find(yParameters[j]) == std::string::npos) isNotRedundant = false;
            }

            if(isNotRedundant and xParameters[i] != yParameters[j]){
                parameterPairs.push_back(xParameters[i] + yParameters[j]);
            
                TH2F* hist2D = new TH2F((xParameters[i] + "_VS_" + yParameters[j]).c_str() , (xParameters[i] + "_VS_" + yParameters[j]).c_str(), binning[confX.func][0], binning[confX.func][1], binning[confX.func][2], binning[confY.func][0], binning[confY.func][1], binning[confY.func][2]);
                hist2D->Sumw2();
                hist2D->SetDirectory(outputFile);

                std::string fLabelY = funcLabel[confY.func];

                for(unsigned int k = 0; k < confY.parts.size(); k++){
                    std::string pLabel = (confY.func != NPART and confY.func != NSIGPART) ? partLabel[confY.parts[k]] : nPartLabel[confY.parts[k]];
                    if(pLabel.find("@") != std::string::npos){
                        pLabel.replace(pLabel.find("@"), 1, std::to_string(confY.indeces[k]));
                    }

                    if(fLabelY.find("@") != std::string::npos){
                        fLabelY.replace(fLabelY.find("@"), 1, pLabel);
                    }
                }

                hist2D->GetXaxis()->SetTitle(fLabelX.c_str());
                hist2D->GetYaxis()->SetTitle(fLabelY.c_str());
                confY.hist2D = hist2D;
                temp2DHist.push_back(confY);
            }
        }

        histograms2D.push_back(temp2DHist);
    }

    return {histograms1D, histograms2D};
}

void TreeReader::ParallelisedLoop(const std::string &fileName, const int &entryStart, const int &entryEnd){
    std::thread::id index = std::this_thread::get_id();
    std::stringstream threadString;  threadString << index;

    TH1::AddDirectory(kFALSE);

    //Lock thread unsafe operation
    mutex.lock();

    //ROOT files
    TFile* inputFile = TFile::Open(fileName.c_str(), "READ");
    TFile* outputFile = TFile::Open(std::string(process + "_" + threadString.str() + ".root").c_str(), "RECREATE");

    //Define containers for histograms
    std::vector<Hist> histograms1D;
    std::vector<std::vector<Hist>> histograms2D;
    TTree* outputTree = new TTree(process.c_str(), process.c_str());
    outputTree->SetDirectory(outputFile);

    //Set up histograms
    std::tie(histograms1D, histograms2D) = SetHistograms(outputFile);

    //vector with values
    std::vector<float> valuesX(histograms1D.size(), 1.);

    //Create thread local histograms/branches
    for(unsigned int i = 0; i < histograms1D.size(); i++){
        if(saveTree) outputTree->Branch(xParameters[i].c_str(), &valuesX[i]);
    }

    //Define TTreeReader 
    TTree* inputTree = (TTree*)inputFile->Get(channel.c_str());
    TTreeReader reader(inputTree);

    TTreeReaderValue<std::vector<Electron>> electrons(reader, "electron");
    TTreeReaderValue<std::vector<Muon>> muons(reader, "muon");
    TTreeReaderValue<std::vector<Jet>> jets(reader, "jet");
    TTreeReaderValue<std::vector<Jet>> subjets(reader, "subjet");
    TTreeReaderValue<std::vector<FatJet>> fatjets(reader, "fatjet");
    TTreeReaderValue<TLorentzVector> MET(reader, "met");
    TTreeReaderValue<float> HT(reader, "HT");
    TTreeReaderValue<float> evtNum(reader, "eventNumber");

    TTreeReaderValue<float> lumi(reader, "lumi");
    TTreeReaderValue<float> xSec(reader, "xsec");
    TTreeReaderValue<float> puWeight(reader, "puWeight");
    TTreeReaderValue<float> genWeight(reader, "genWeight");

    //Number of generated events
    float nGen = 1.;

    if(inputFile->GetListOfKeys()->Contains("nGen")){
        TH1F* genHist = (TH1F*)inputFile->Get("nGen");
        nGen = genHist->Integral();
        delete genHist;
    }

    //Cutflow
    TH1F* cutflow = (TH1F*)inputFile->Get(("cutflow_" + channel).c_str())->Clone();
    cutflow->SetName("cutflow"); cutflow->SetTitle("cutflow");
    cutflow->Scale((1./nGen)*(entryEnd-entryStart)/inputTree->GetEntries());

    //BDT intialization
    if(isBDT){
        std::map<std::string, std::string> chanPaths = {
                    {"e4j", "Ele4J"},
                    {"mu4j", "Muon4J"},
                    {"e2j1f", "Ele2J1F"},
                    {"mu2j1f", "Muon2J1F"},
                    {"e2f", "Ele2F"},
                    {"mu2f", "Muon2F"},
        };

        std::string bdtPath = std::string(std::getenv("CMSSW_BASE")) + "/src/BDT/" + chanPaths[channel]; 

        std::vector<std::string> bdtVar = evenClassifier[index].SetEvaluation(bdtPath + "/Even/");
        oddClassifier[index].SetEvaluation(bdtPath + "/Odd/");

        bdtVar.pop_back();

        for(std::string param: bdtVar){
            bdtFunctions[index].push_back(ConvertStringToEnums(param));
        }
    }

    mutex.unlock();

    Event event;

    for (int i = entryStart; i < entryEnd; i++){
        //Load event and fill event class
        reader.SetEntry(i); 

        event = {*electrons, *muons, *jets, *subjets, *fatjets, *MET, 1., *HT, *evtNum}; 

        //Fill additional weights
        std::vector<float> weightVec = {*genWeight < 2.f and *genWeight > 0.2 ? *genWeight: 1.f, *puWeight, *lumi, *xSec, 1.f/nGen};

        for(const float &weight: weightVec){
            event.weight *= weight;
        }

        //Check if event passes cut
        bool passedCut = true;
 
        for(unsigned int k = 0; k < cuts.size(); k++){
            passedCut = Cut(event, cuts[k]);

            //Fill Cutflow if event passes selection
            if(passedCut){
                cutflow->Fill(cutNames[k].c_str(), event.weight);
            }

            else{break;}
        }   

        if(passedCut){
            valuesX.clear();

            for(unsigned int l = 0; l < histograms1D.size(); l++){
                valuesX.push_back((this->*funcDir[histograms1D[l].func])(event, histograms1D[l]));
                histograms1D[l].hist1D->Fill(valuesX[l], event.weight);

               for(unsigned int m = 0; m < histograms2D[l].size(); m++){
                    histograms2D[l][m].hist2D->Fill(valuesX[l], (this->*funcDir[histograms2D[l][m].func])(event, histograms2D[l][m]), event.weight);
                }
            }

            //Fill tree if wished
            if(saveTree) outputTree->Fill();
        }
    }

    //Lock again and merge local histograms into final histograms
    mutex.lock();

    //Write histograms
    outputFile->cd();
        
    if(writeCutFlow) cutflow->Write();
    if(saveTree) outputTree->Write();
    
    else{
        for(unsigned int l = 0; l < histograms1D.size(); l++){
            histograms1D[l].hist1D->Write();
            delete histograms1D[l].hist1D;

            for(unsigned int m = 0; m < histograms2D[l].size(); m++){
                histograms2D[l][m].hist2D->Write();
                delete histograms2D[l][m].hist2D;
            }
        }
    }

    delete inputTree;
    delete outputTree;
    delete outputFile;
    delete inputFile;
    delete cutflow;

    //Progress bar
    if(nJobs != 0){
        progress += 100*(1.f/nJobs);
        ProgressBar(progress);
    }

    mutex.unlock();
}

std::vector<std::vector<std::pair<int, int>>> TreeReader::EntryRanges(std::vector<std::string> &filenames, int &nJobs, std::string &channel, const float &frac){
    std::vector<int> jobsPerFile(filenames.size(), 0); 

    //Calculate number of jobs per file
    for(unsigned int i = 0; i < jobsPerFile.size(); i++){
        for(int j = 0; j < std::floor(nJobs/jobsPerFile.size()); j++){
             jobsPerFile[i]++;
        }
    }

    std::vector<std::vector<std::pair<int, int>>> entryRange(filenames.size(), std::vector<std::pair<int, int>>());

    for(unsigned int i = 0; i < filenames.size(); i++){
        //Get nGen for each file
        TFile* file = TFile::Open(filenames[i].c_str());
        TTree* tree = NULL;

        if(!file->GetListOfKeys()->Contains(channel.c_str())){
            std::cout << file->GetName() << " has no event tree. It will be skipped.." << std::endl;
        }

        else tree = (TTree*)file->Get(channel.c_str());

        for(int j = 0; j < jobsPerFile[i]; j++){
            if(j != jobsPerFile[i] - 1 and tree != NULL){
                entryRange[i].push_back({j*tree->GetEntries()*frac/jobsPerFile[i], (j+1)*tree->GetEntries()*frac/jobsPerFile[i]});
            }

            else if(tree != NULL){
                entryRange[i].push_back({j*tree->GetEntries()*frac/jobsPerFile[i], tree->GetEntries()*frac});
            }

            else{
                entryRange[i].push_back({-1., -1.});
            }
        }
    }

    return entryRange;
}

void TreeReader::Run(std::vector<std::string> &filenames, const float &frac){
    //Configure threads
    nJobs = (int)std::thread::hardware_concurrency();   

    std::vector<std::thread> threads;
    int assignedCores = 0;

    //Get entry ranges
    std::vector<std::vector<std::pair<int, int>>> entries = EntryRanges(filenames, nJobs, channel, frac);

    for(unsigned int i = 0; i < filenames.size(); i++){
        for(std::pair<int, int> range: entries[i]){
            if(range.first == -1.) continue;

            threads.push_back(std::thread(&TreeReader::ParallelisedLoop, this, filenames[i], range.first, range.second));

            //Set for each thread one core
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(assignedCores, &cpuset);

            pthread_setaffinity_np(threads[assignedCores].native_handle(), sizeof(cpu_set_t), &cpuset);

            assignedCores++;
        }
    }

    //Progress bar at 0 %
    ProgressBar(progress);

    //Let it run
    for(std::thread &thread: threads){
        thread.join();
    }

    //Progress bar at 100%
    ProgressBar(100);

    end = std::chrono::steady_clock::now();
    std::cout << "Created output for process:" << process << " (" << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << " s)" << std::endl;
}

void TreeReader::Run(std::string &fileName, int &entryStart, int &entryEnd){
    ParallelisedLoop(fileName, entryStart, entryEnd);

    end = std::chrono::steady_clock::now();
    std::cout << "Created output for process:" << process << " (" << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << " s)" << std::endl;
}

void TreeReader::Merge(){
    std::system(std::string("hadd -f " + outname + " " + process + "_*").c_str());
    std::system(std::string("command rm " + process + "_*").c_str());

    TSeqCollection* fileList = gROOT->GetListOfFiles();

    for(int i=0; i < fileList->GetSize(); i++){
        delete fileList->At(i);
    }
}
