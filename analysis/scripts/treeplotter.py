#!/usr/bin/env python

import ROOT
ROOT.PyConfig.IgnoreCommandLineOptions = True

import os
import yaml
import argparse

from multiprocessing import Pool, cpu_count

def parser():
    parser = argparse.ArgumentParser()
    
    parser.add_argument("--background", nargs="+", default = ["TT+X", "T+X", "QCD", "W+j", "DY+j", "VV+VVV"], help = "Name of background processes")
    parser.add_argument("--data", nargs="+", default = [], help = "Name of data process")
    parser.add_argument("--signal", nargs = "+", default = [], help = "Name of data process")   

    parser.add_argument("--skim-dir", type = str, help = "Directory with skimmed files")
    parser.add_argument("--process-yaml", type = str, default = "{}/src/ChargedHiggs/analysis/data/process.yaml".format(os.environ["CMSSW_BASE"]), help = "Yaml configuration which maps process name and skimmed files names")
    parser.add_argument("--plot-dir", type = str, default = "{}/src/Plots".format(os.environ["CMSSW_BASE"]), help = "Output directory of plots")

    parser.add_argument("--channel", type = str, required = True, help = "Final state which is of interest")
    parser.add_argument("--x-parameters", nargs="+", default = [], help = "Name of parameters to be plotted")
    parser.add_argument("--y-parameters", nargs="+", default = [], help = "Y parameters for 2D plots")
    parser.add_argument("--passed-trigger", nargs="+", help = "Parameter for ref trigger and main trigger for plotting trigger eff")
    parser.add_argument("--total-trigger", type = str, help = "Parameter for ref trigger for plotting trigger eff")
    parser.add_argument("--cuts", nargs="+", default=[], help = "Name of cut implemented in TreeReader class")

    parser.add_argument("--hist-dir", type = str, default = "{}/src/Histograms".format(os.environ["CMSSW_BASE"]), help = "Dir with histograms read out of skimmed trees")
    parser.add_argument("--www", type=str, help = "Dir to web plot dir")

    return parser.parse_args()

def createHistograms(process, filenames, xParameters, yParameters, cuts, outdir, channel):
    outname = ROOT.std.string("{}/{}.root".format(outdir, process))

    reader = ROOT.TreeReader(ROOT.std.string(process), xParameters, yParameters, cuts, outname)
    reader.SetHistograms()
    reader.EventLoop(filenames, ROOT.std.string(channel))
    reader.Write()

def makePlotsTriggerEff(histDir, trigger, processes, plotDir, channel, yParameters):

    totalTrigger = ROOT.std.string(trigger[0])
    passedTrigger = ROOT.std.vector("string")()

    for trig in trigger:
        if "and" in trig:
            passedTrigger.push_back(trig)

    plotter = ROOT.PlotterTriggEff(histDir, totalTrigger, passedTrigger, yParameters, ROOT.std.string(channel))
    plotter.ConfigureHists(processes)
    plotter.SetStyle()
    plotter.Draw(plotDir)

def makePlots1D(histDir, xParameters, processes, plotDir, channel):

    plotter = ROOT.Plotter1D(histDir, xParameters, ROOT.std.string(channel))
    plotter.ConfigureHists(processes)
    plotter.SetStyle()
    plotter.Draw(plotDir)

def makePlots2D(histDir, xParameters, yParameters, processes, plotDir, channel):

    plotter = ROOT.Plotter2D(histDir, xParameters, yParameters, ROOT.std.string(channel))
    plotter.ConfigureHists(processes)
    plotter.SetStyle()
    plotter.Draw(plotDir)
    
def main():
    ##Parser
    args = parser()

    ##Load filenmes of processes
    process_dic = yaml.load(file(args.process_yaml, "r"))

    ##Create plot/hist dir
    if not os.path.exists(args.hist_dir):
        print "Created output path: {}".format(args.hist_dir)
        os.system("mkdir -p {}".format(args.hist_dir))

    if not os.path.exists(args.plot_dir):
        print "Created output path: {}".format(args.plot_dir)
        os.system("mkdir -p {}".format(args.plot_dir))

    if args.www:
        if not os.path.exists(args.www):
            print "Created output path: {}".format(args.www)
            os.system("mkdir -p {}".format(args.www))

    ##Create std::vector for parameters
    xParameters = ROOT.std.vector("string")()
    [xParameters.push_back(param) for param in args.x_parameters]

    yParameters = ROOT.std.vector("string")()
    [yParameters.push_back(param) for param in args.y_parameters]

    if args.passed_trigger:
        trigger = ROOT.std.vector("string")()
        [trigger.push_back(param) for param in [args.total_trigger] + args.passed_trigger]

    processes = ROOT.std.vector("string")()
    [processes.push_back(proc) for proc in args.background + args.data + args.signal]

    cuts = ROOT.std.vector("string")()
    [cuts.push_back(cut) for cut in args.cuts]

    histDir = ROOT.std.string(args.hist_dir)
    outDir = ROOT.std.vector("string")()
    outDir.push_back(args.plot_dir)

    if args.www:
        outDir.push_back(args.www)

    ##Create histograms
    for process in processes:
        filenames = ROOT.std.vector("string")()
        
        [filenames.push_back(fname) for fname in ["{skim}{file}/{file}.root".format(skim = args.skim_dir, file = process_file) for process_file in process_dic[process]["filenames"]]]       

        if args.x_parameters:
            createHistograms(process, filenames, xParameters, yParameters, cuts, histDir, args.channel)

        elif args.passed_trigger:
            yParameters = ROOT.std.vector("string")()
            [yParameters.push_back(param) for param in {"ele+4j": ["e_pt", "j1_pt", "HT"], "mu+4j": ["mu_pt"]}[args.channel]]

            createHistograms(process, filenames, trigger, yParameters, cuts, histDir, args.channel)
                   
        else:
            print "No parameter input, no histograms can be produced"
            return 0
    
    if args.passed_trigger:
        makePlotsTriggerEff(histDir, trigger, processes, outDir, args.channel, yParameters, args.channel)

    if args.x_parameters:
        pass
        makePlots1D(histDir, xParameters, processes, outDir, args.channel)

    if args.y_parameters:
        makePlots2D(histDir, xParameters, yParameters, processes, outDir, args.channel)

    if args.www:
        os.system("rsync -arvt --exclude {./,.git*} --delete -e ssh $HOME2/webpage/ $CERN_USER@lxplus.cern.ch:/eos/home-${USER:0:1}/$CERN_USER/www/")
        print "Plots are avaiable on: https://dbrunner.web.cern.ch/dbrunner/"
        

if __name__ == "__main__":
    main()
