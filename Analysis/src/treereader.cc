#include <ChargedAnalysis/Analysis/include/treereader.h>

//Constructor

TreeReader::TreeReader(){}

TreeReader::TreeReader(const std::vector<std::string> &parameters, const std::vector<std::string> &cutStrings, const std::string &outname, const std::string &channel):
    parameters(parameters),
    cutStrings(cutStrings),
    outname(outname),
    channel(channel){}

void TreeReader::PrepareLoop(TFile* outFile, TTree* inputTree){
    TreeParser parser;

    for(const std::string& parameter: parameters){
        //Functor structure and arguments
        TreeFunction function(inputTree->GetCurrentFile(), inputTree->GetName());
        
        //Read in everything, orders matter
        parser.GetParticle(parameter, function);
        parser.GetFunction(parameter, function);

        if(Utils::Find<std::string>(parameter, "h:") != -1){
            if(!function.hasYAxis()){
                TH1F* hist1D = new TH1F();
                parser.GetBinning(parameter, hist1D);

                hist1D->SetName((function.GetName<Axis::X>()).c_str());       
                hist1D->SetTitle((function.GetName<Axis::X>()).c_str());
                hist1D->SetDirectory(outFile);

                hist1D->GetXaxis()->SetTitle(function.GetAxisLabel<Axis::Y>().c_str());

                hists1D.push_back(hist1D);
                hist1DFunctions.push_back(function);    
            }

            else{
                TH2F* hist2D = new TH2F();
                parser.GetBinning(parameter, hist2D);

                hist2D->SetName((function.GetName<Axis::X>() + "_VS_" + function.GetName<Axis::Y>()).c_str());       
                hist2D->SetTitle((function.GetName<Axis::X>() + "_VS_" + function.GetName<Axis::Y>()).c_str());
                hist2D->SetDirectory(outFile);

                hist2D->GetXaxis()->SetTitle(function.GetAxisLabel<Axis::X>().c_str());
                hist2D->GetYaxis()->SetTitle(function.GetAxisLabel<Axis::Y>().c_str());

                hists2D.push_back(hist2D);
                hist2DFunctions.push_back(function);    
            }
        }

        if(Utils::Find<std::string>(parameter, "t:") != -1){
            if(outTree == NULL){
                outTree = new TTree(channel.c_str(), channel.c_str());
                outTree->SetDirectory(outFile);
            }

            branchNames.push_back(function.GetName<Axis::X>());
            treeValues.push_back(1.);

            treeFunctions.push_back(function);
        }

        if(Utils::Find<std::string>(parameter, "csv:") != -1){
            CSVNames.push_back(function.GetName<Axis::X>());

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
        TreeFunction function(inputTree->GetCurrentFile(), inputTree->GetName());

        parser.GetParticle(cut, function);
        parser.GetFunction(cut, function);
        parser.GetCut(cut, function);

        cutFunctions.push_back(function);

        std::cout << "Cut will be applied: '" << function.GetCutLabel() << "'" << std::endl;
    }
}

void TreeReader::EventLoop(const std::string &fileName, const int &entryStart, const int &entryEnd, const std::string& cleanJet){
    //Take time
    Utils::RunTime timer;

    //Get input tree
    TFile* inputFile = Utils::CheckNull<TFile>(TFile::Open(fileName.c_str(), "READ"));
    TTree* inputTree = Utils::CheckNull<TTree>(inputFile->Get<TTree>(channel.c_str()));

    TLeaf* nTrueInter = inputTree->GetLeaf("Misc_TrueInteraction");

    std::cout << "Read file: '" << fileName << "'" << std::endl;
    std::cout << "Read tree '" << channel << "' from " << entryStart << " to " << entryEnd << std::endl;

    gROOT->SetBatch(kTRUE);

    std::string name = outname;
    if(name.find("csv") != std::string::npos) name.replace(name.find("csv"), 3, "root");

    //Open output file and set up all histograms/tree and their function to call
    TFile* outFile = TFile::Open(name.c_str(), "RECREATE");
    PrepareLoop(outFile, inputTree);

    //Determine what to clean from jets
    std::vector<std::string> cleanInfo = Utils::SplitString<std::string>(cleanJet, "/");

    if(cleanInfo.size() != 1){
        for(int j=0; j < hist1DFunctions.size(); j++){
            hist1DFunctions[j].SetCleanJet<Axis::X>(cleanInfo[0], cleanInfo[1]);
        }

        for(int j=0; j < hist2DFunctions.size(); j++){
            hist2DFunctions[j].SetCleanJet<Axis::X>(cleanInfo[0], cleanInfo[1]);
            hist2DFunctions[j].SetCleanJet<Axis::Y>(cleanInfo[0], cleanInfo[1]);
        }

        for(int j=0; j < cutFunctions.size(); j++){
            cutFunctions[j].SetCleanJet<Axis::X>(cleanInfo[0], cleanInfo[1]);
        }

        for(int j=0; j < treeFunctions.size(); j++){
            treeFunctions[j].SetCleanJet<Axis::X>(cleanInfo[0], cleanInfo[1]);
        }

        for(int j=0; j < CSVFunctions.size(); j++){
            CSVFunctions[j].SetCleanJet<Axis::X>(cleanInfo[0], cleanInfo[1]);
        }
    }

    //Get number of generated events
    if(inputFile->GetListOfKeys()->Contains("nGen")){
        nGen = inputFile->Get<TH1F>("nGen")->Integral();
    }

    if(inputFile->GetListOfKeys()->Contains("xSec")){
        xSec = inputFile->Get<TH1F>("xSec")->GetBinContent(1);
    }

    if(inputFile->GetListOfKeys()->Contains("Lumi")){
        lumi = inputFile->Get<TH1F>("Lumi")->GetBinContent(1);
    }

    //Set all branch addresses needed for the event
    bool passed = true;
    bool isData = true;

    //Calculate pile up weight histogram
    TH1F* pileUpWeight=nullptr;
    TH1F* pileUpWeightUp=nullptr;
    TH1F* pileUpWeightDown=nullptr;

    if(inputFile->GetListOfKeys()->Contains("puMC")){
        isData = false;
        TH1F* puMC = (TH1F*)inputFile->Get("puMC");
        TH1F* puReal = (TH1F*)inputFile->Get("pileUp");
        TH1F* puRealUp = (TH1F*)inputFile->Get("pileUpUp");
        TH1F* puRealDown = (TH1F*)inputFile->Get("pileUpDown");

        pileUpWeight = (TH1F*)puReal->Clone();
        pileUpWeightUp = (TH1F*)puRealUp->Clone();
        pileUpWeightDown = (TH1F*)puRealDown->Clone();

        pileUpWeight->Scale(1./pileUpWeight->Integral());
        pileUpWeightUp->Scale(1./pileUpWeightUp->Integral());
        pileUpWeightDown->Scale(1./pileUpWeightDown->Integral());
        puMC->Scale(1./puMC->Integral());

        pileUpWeight->Divide(puMC);
        pileUpWeightUp->Divide(puMC);
        pileUpWeightDown->Divide(puMC);
        delete puMC; delete puReal;
    }

    //Weight with all variations
    std::vector<float> weight(xSec*lumi, std::accumulate(cutFunctions.begin(), cutFunctions.end(), 2, [&](int v, TreeFunction cut){return v + 2*cut.GetNWeights();}));

    //Cutflow
    TH1F* cutflow = inputFile->Get<TH1F>(("cutflow_" + channel).c_str());
    cutflow->SetName("cutflow"); cutflow->SetTitle("cutflow");
    if(inputTree->GetEntries() != 0) cutflow->Scale((1./nGen)*(entryEnd-entryStart)/inputTree->GetEntries());
    else cutflow->Scale(1./nGen);

    for (int i = entryStart; i < entryEnd; i++){
        if(i % 100000 == 0 and i != 0){
            std::cout << "Processed events: " << i-entryStart << " (" << (i-entryStart)/timer.Time() << " eve/s)" << std::endl;
        }

        //Set Entry
        TreeFunction::SetEntry(i);
        nTrueInter->GetBranch()->GetEntry(i);

        //Multiply all common weights
        float weight = 1./nGen;
        if(pileUpWeight!=nullptr) weight *= pileUpWeight->GetBinContent(pileUpWeight->FindBin(*(float*)nTrueInter->GetValuePointer()));

        //Check if event passed all cuts
        passed=true;

        for(int j=0; j < cutFunctions.size(); j++){
            passed = passed && cutFunctions[j].GetPassed();

            if(passed){
                weight *= cutFunctions[j].GetWeight();
                cutflow->Fill(cutFunctions[j].GetCutLabel().c_str(), weight);
            }

            else break;
        }

        if(!passed) continue;

        //Fill histogram
        for(int j=0; j < hists1D.size(); j++){
            hists1D[j]->Fill(hist1DFunctions[j].Get<Axis::X>(), weight);
        }

        for(int j=0; j < hists2D.size(); j++){
            hists2D[j]->Fill(hist2DFunctions[j].Get<Axis::X>(), hist2DFunctions[j].Get<Axis::Y>(), weight);
        }

        //Fill trees
        for(int j=0; j < treeFunctions.size(); j++){
            treeValues[j] = treeFunctions[j].Get<Axis::X>();
        }

        if(outTree != nullptr) outTree->Fill();

        //Fill CSV file
        if(frame != nullptr){
            std::vector<float> values;

            for(int j=0; j < CSVFunctions.size(); j++){
                values.push_back(CSVFunctions[j].Get<Axis::X>());
            }

            frame->AddColumn(values);
        }
    }

    //Write all histograms and delete everything
    outFile->cd();
    for(TH1F* hist: hists1D){
        hist->Write(); 
        std::cout << "Saved histogram: '" << hist->GetName() << "' with " << hist->GetEntries() << " entries" << std::endl;
        delete hist;
    }

    for(TH2F* hist: hists2D){
        hist->Write(); 
        std::cout << "Saved histogram: '" << hist->GetName() << "' with " << hist->GetEntries() << " entries" << std::endl;
        delete hist;
    }

    if(outTree != nullptr){
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
    std::cout << "Time passed for complete processing: " << timer.Time() << " s" << std::endl;
}
