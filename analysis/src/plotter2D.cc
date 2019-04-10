#include <ChargedHiggs/analysis/interface/plotter2D.h>

Plotter2D::Plotter2D() : Plotter(){}


Plotter2D::Plotter2D(std::string &histdir, std::vector<std::string> &xParameters, std::vector<std::string> &yParameters, std::string &channel) :
    Plotter(histdir, xParameters, yParameters, channel),
    background({}),
    signal({}),
    data({})
 {}


void Plotter2D::ConfigureHists(std::vector<std::string> &processes){
    for(std::string xParameter: xParameters){
        //Define Vector for processes
        std::vector<std::vector<TH2F*>> bkgHistVec;
        std::vector<std::vector<TH2F*>> sigHistVec;

        for(std::string yParameter: yParameters){

            std::vector<TH2F*> bkgHists;
            std::vector<TH2F*> sigHists;

            for(std::string process: processes){
                //Get Histogram for parameter
                std::string filename = histdir + "/" + process + ".root";
                TFile* file = TFile::Open(filename.c_str());
                TH2F* hist = (TH2F*)file->Get((xParameter + "_VS_" + yParameter).c_str());

                if(procDic[process] == BKG){
                    hist->SetMarkerColor(kRed);
                    hist->SetMarkerStyle(20);
                    bkgHists.push_back(hist);
                }

                if(procDic[process] == SIGNAL){
                    hist->SetMarkerColor(kBlue);
                    hist->SetMarkerStyle(20);
                    sigHists.push_back(hist);
                }
            }
            
            sigHistVec.push_back(sigHists);
            bkgHistVec.push_back(bkgHists);
        }

        signal.push_back(sigHistVec);
        background.push_back(bkgHistVec);
    }
}

void Plotter2D::Draw(std::vector<std::string> &outdirs){
    TCanvas *canvas = new TCanvas("canvas", "canvas", 1000, 800); 

    for(unsigned int i = 0; i < xParameters.size(); i++){
        for(unsigned int j = 0; j < yParameters.size(); j++){
            TH2F* bkgSum =  (TH2F*)background[i][j][0]->Clone();
            bkgSum->Clear();

            for(TH2F* hist: background[i][j]){
                bkgSum->Add(hist);
            }
            
            canvas->Clear();
            bkgSum->DrawNormalized("COLZ");
            this->DrawHeader(false, channelHeader[channel], "Work in progress");

            for(std::string outdir: outdirs){
                canvas->SaveAs((outdir + "/" + xParameters[i] + "_VS_" + yParameters[j] + "_bkg.pdf").c_str());
                canvas->SaveAs((outdir + "/" + xParameters[i] + "_VS_" + yParameters[j] + "_bkg.png").c_str());
            }

            canvas->Clear();
            signal[i][j][0]->DrawNormalized("COLZ");
            this->DrawHeader(false, channelHeader[channel], "Work in progress");

            for(std::string outdir: outdirs){
                canvas->SaveAs((outdir + "/" + xParameters[i] + "_VS_" + yParameters[j] + "_sig.pdf").c_str());
                canvas->SaveAs((outdir + "/" + xParameters[i] + "_VS_" + yParameters[j] + "_sig.png").c_str());
            }

            std::cout << "Plot created for: " << xParameters[i] << " VS " << yParameters[j] << std::endl;
        }   
    }
    
    delete canvas;
}
