#!/usr/bin/env python3

from taskmanager import TaskManager
from configuration import *

import os
import argparse
import yaml
import copy
import sys
import numpy

from collections import defaultdict as dd

def parser():
    parser = argparse.ArgumentParser(description = "Script to handle and execute analysis tasks", formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("--task", type=str, choices = ["Plot", "HTagger", "Append", "Limit", "DNN", "Index", "Merge", "Estimate", "Stitch", "Fakerate", "NanoSkim"])
    parser.add_argument("--config", type=str)
    parser.add_argument("--era", nargs='+', default = [""])
    parser.add_argument('--dry-run', default=False, action='store_true')
    parser.add_argument("--run-again", type=str)
    parser.add_argument("--long-condor", default=False, action='store_true')
    parser.add_argument("--global-mode", type=str, default = "")
    parser.add_argument("--n-cores", default=None, type=int)

    return parser.parse_args()

def taskReturner(taskName, config):
    tasks = {
        "Append": lambda : append(config),
        "HTagger": lambda : htagger(config),
        "Index": lambda : index(config),
        "DNN": lambda : dnn(config),
        "Plot": lambda : plot(config),
        "Limit": lambda : limit(config),
        "Merge": lambda: mergeskim(config),
        "Estimate": lambda : estimate(config),
        "Stitch": lambda: stitch(config),
        "Fakerate": lambda: calcFR(config),
        "NanoSkim": lambda : nanoskim(config),
    }

    return tasks[taskName]

def index(config):
    tasks = []

    for channel in config["channels"]:
        for era in config["era"]:
            processes = sorted(config["processes"] + config.get("data", {}).get(channel, []))
            shapeSysts = config["shape-systs"].get("all", [""]) + config["shape-systs"].get(channel, [])

            for process in processes:
                for syst in shapeSysts:
                    for shift in ["Up", "Down"]:
                        treeIndex(tasks, config, channel, era, process, syst, shift)
                        mergeCSV(tasks, config, channel, era, process, syst, shift)

    return tasks

def plot(config):
    tasks = []

    for channel in config["channels"]:
        for era in config["era"]:
            processes = sorted(config["processes"] + config.get("data", {}).get(channel, []))

            shapeSysts = config["shape-systs"].get("all", [""]) + config["shape-systs"].get(channel, [])
            allSysts = shapeSysts + config["scale-systs"].get("all", []) + config["scale-systs"].get(channel, [])

            for process in processes:
                if config.get("estimate-QCD", False) and process == "QCD":
                    continue

                for syst in shapeSysts:
                    for shift in ["Up", "Down"]:
                        treeread(tasks, config, channel, era, process, syst, shift, postFix = "Baseline")

                for syst in allSysts:
                    for shift in ["Up", "Down"]:
                        mergeHists(tasks, config, channel, era, process, syst, shift, postFix = "Baseline")

            if config.get("estimate-QCD", False):
                for syst in allSysts:
                    for shift in ["Up", "Down"]:
                        QCDEstimation(tasks, config, channel, era, syst, shift, "Baseline")

            plotting(tasks, config, channel, era, processes, postFix = "Baseline")

    return tasks

def mergeskim(config):
    tasks = []

    for channel in config["channels"]:
        for era in config["era"]:
            for syst in config["systematics"]:
                for shift in ["Up", "Down"]:
                    if config.get("is-nano", True):
                        mergeNanoSkim(tasks, config, channel, era, syst, shift)

                    else:
                        mergeSkim(tasks, config, channel, era, syst, shift)

    return tasks

def dnn(config):
    tasks = []

    for channel in config["channels"]:
        for era in config["era"]:
            for evType in ["Even", "Odd"]:
                DNN(tasks, config, channel, era, evType)

    return tasks

def append(config):
    tasks = []

    for channel in config["channels"]:
        for era in config["era"]:
            for syst in config["shape-systs"].get("all", []) + config["shape-systs"].get(channel, []):
                for shift in ["Up", "Down"]:
                    treeappend(tasks, config, channel, era, syst, shift)
    
    return tasks

def estimate(config):
    tasks = []

    for channel in config["channels"]:
        for era in config["era"]:
            for mass in config["charged-masses"]:
                shapeSysts = config["shape-systs"].get("all", [""]) + config["shape-systs"].get(channel, [])
                allSysts = shapeSysts + config["scale-systs"].get("all", []) + config["scale-systs"].get(channel, [])

                for process in config["estimate-process"]:
                    c = copy.deepcopy(config)
                    c["dir"] = "{}/{}-Region".format(config["dir"], process).replace("{MHC}", str(mass))

                    for chan in [channel, "all"]:
                        for cut in config["estimate-process"][process][chan]:
                            c["cuts"].setdefault("all", []).append(cut.format_map(dd(str, {"MHC": mass})))
                                        
                    processes = config["processes"] + config["data"][channel]

                    for procc in processes:
                        for syst in shapeSysts:
                            for shift in ["Up", "Down"]:
                                treeread(tasks, c, channel, era, procc, syst, shift, postFix = "{}_{}".format(process, str(mass)))

                        for syst in allSysts:
                            for shift in ["Up", "Down"]:
                                mergeHists(tasks, c, channel, era, procc, syst, shift, postFix = "{}_{}".format(process, str(mass)))

                    plotting(tasks, c, channel, era, processes, postFix = "{}_{}".format(process, str(mass)))

                    for syst in allSysts:
                        for shift in ["Up", "Down"]:
                            eventCount(tasks, c, channel, era, processes, syst, shift, postFix = "{}_{}".format(process, str(mass)))
                    
                for syst in allSysts:
                    for shift in ["Up", "Down"]:    
                        bkgEstimation(tasks, config, channel, era, str(mass), syst, shift)

    return tasks

def stitch(config):
    tasks = []

    for channel in config["channels"]:
        for era in config["era"]:
            for syst in config["shape-systs"].get("all", [""]) + config["shape-systs"].get(channel, []):
                for shift in ["Up", "Down"]:
                    stitching(tasks, config, channel, era, syst, shift)

    return tasks

def limit(config):
    tasks = []

    for mHPlus in config["charged-masses"]:
        for mH in config["neutral-masses"]:
            conf = copy.deepcopy(config)
            conf["signal"] = config["signal"].format_map(dd(str, {"MHC": mHPlus, "MH": mH}))
            conf["discriminant"] = config["discriminant"].format_map(dd(str, {"MHC": mHPlus, "MH": mH}))

            ##Produce limits for each era/channel individually
            for channel in config["channels"]:
                for era in config["era"]:
                    for region in ["SR", "VR"] + config.get("control-regions", []):
                        conf["dir"] = "{}/{}-Region".format(config["dir"].replace("{MHC}", str(mHPlus)).replace("{MH}", str(mH)), region)
                        conf["cuts"]["all"] = [c.replace("{MHC}", str(mHPlus)) for c in config["cuts"]["all"] + config["cuts"][region]]
                        processes = conf["backgrounds"] + [conf["signal"]] + conf.get("data", {}).get(channel, [])

                        shapeSysts = config["shape-systs"].get("all", [""]) + config["shape-systs"].get(channel, [])
                        allSysts = shapeSysts + config["scale-systs"].get("all", []) + config["scale-systs"].get(channel, [])

                        for procc in processes:
                            for syst in shapeSysts:
                                for shift in ["Up", "Down"]:
                                    treeread(tasks, conf, channel, era, procc, syst, shift, mHPlus, postFix = "{}_{}_{}Region".format(mHPlus, mH, region))
                            
                            for syst in allSysts:
                                for shift in ["Up", "Down"]:
                                    mergeHists(tasks, conf, channel, era, procc, syst, shift, postFix = "{}_{}_{}Region".format(mHPlus, mH, region))

                        plotting(tasks, conf, channel, era, processes, postFix = "{}_{}_{}Region".format(mHPlus, mH, region))

                        for syst in allSysts:
                            for shift in ["Up", "Down"]:
                                eventCount(tasks, conf, channel, era, processes, syst, shift, postFix = "{}_{}_{}Region".format(mHPlus, mH, region))

                    conf["dir"] = config["dir"].replace("{MHC}", str(mHPlus)).replace("{MH}", str(mH))

                    datacard(tasks, conf, channel, era, processes, mHPlus, mH)
                    limits(tasks, conf, channel, era, mHPlus, mH)
                    plotpostfit(tasks, conf, channel, era, mHPlus, mH)
                    plotimpact(tasks, conf, channel, era, mHPlus, mH)

            ##Produce limits for each era and all channel combined
            for era in config["era"]:
                mergeDataCards(tasks, conf, {"Combined": config["channels"]}, era, mHPlus, mH)
                limits(tasks, conf, "Combined", era, mHPlus, mH)
                plotpostfit(tasks, conf, "Combined", era, mHPlus, mH)                    
                plotimpact(tasks, conf, "Combined", era, mHPlus, mH)

            ##Produce limits for RunII and each channel
            for channel in config["channels"]:
                mergeDataCards(tasks, conf, channel, {"RunII": config["era"]}, mHPlus, mH)
                limits(tasks, conf, channel, "RunII", mHPlus, mH)
                plotpostfit(tasks, conf, channel, "RunII", mHPlus, mH)
                plotimpact(tasks, conf, channel, "RunII", mHPlus, mH)

            ##Produce limits for RunII and all channel combined
            mergeDataCards(tasks, conf, {"Combined": config["channels"]}, {"RunII": config["era"]}, mHPlus, mH)
            limits(tasks, conf, "Combined", "RunII", mHPlus, mH)
            plotpostfit(tasks, conf, "Combined", "RunII", mHPlus, mH)
            plotimpact(tasks, conf, "Combined", "RunII", mHPlus, mH)

    ##Plot limits for all combination of era/channel
    conf = copy.deepcopy(config)
    conf["dir"] = config["dir"].replace("{MHC}", "").replace("{MH}", "")

    for era in config["era"]:
        for channel in config["channels"]:
            plotlimit(tasks, conf, channel, era)

    for era in config["era"]:
        plotlimit(tasks, config, "Combined", era)

    for channel in config["channels"]:
        plotlimit(tasks, config, channel, "RunII")

    plotlimit(tasks, config, "Combined", "RunII")

    return tasks

def calcFR(config):
    tasks = []

    for channel in config["channels"]:
        for era in config["era"]:
            processes = sorted(config["processes"] + config.get("data", {}).get(channel, []))

            shapeSysts = config["shape-systs"].get("all", [""]) + config["shape-systs"].get(channel, [])
            allSysts = shapeSysts + config["scale-systs"].get("all", []) + config["scale-systs"].get(channel, [])

            for process in processes:
                for syst in shapeSysts:
                    for shift in ["Up", "Down"]:
                        treeread(tasks, config, channel, era, process, syst, shift, postFix = "FR")

                for syst in allSysts:
                    for shift in ["Up", "Down"]:
                        mergeHists(tasks, config, channel, era, process, syst, shift, postFix = "FR")

            plotting(tasks, config, channel, era, processes, postFix = "FR")

            for mode in config["modes"]:
                for syst in allSysts:
                    for shift in ["Up", "Down"]:
                        fakerate(tasks, config, channel, era, mode, syst, shift)

    return tasks

def nanoskim(config):
    tasks = []

    for era in config["era"]:
        nanoSkim(tasks, config, era)

    return tasks

def main():
    args = parser()

    tasks = []
    taskDir = ""

    ##Read general config and update with given config
    if args.config:
        confDir = "{}/ChargedAnalysis/Workflow/config/".format(os.environ["CHDIR"])

        config = yaml.load(open("{}/general.yaml".format(confDir), "r"), Loader=yaml.Loader)
        config.update(yaml.load(open("{}/{}.yaml".format(confDir, args.config), "r"), Loader=yaml.Loader))

        taskDir = "{}/Results/{}/{}".format(os.environ["CHDIR"], args.task, config["dir"].split("/")[0])
        os.makedirs(taskDir, exist_ok = True)

        with open("{}/config.yaml".format(taskDir), "w") as conf:
            yaml.dump(config, conf, default_flow_style=False, indent=4)

        config["dir"] = "{}/Results/{}/{}".format(os.environ["CHDIR"], args.task, config["dir"])
        config["era"] = args.era

        ##Configure and get the tasks
        tasks = taskReturner(args.task, config)()

    elif args.run_again:
        taskDir = "/".join(args.run_again.split("/")[:-1])

    ##Run the manager
    manager = TaskManager(tasks = tasks, existingFlow = args.run_again, dir = taskDir, longCondor = args.long_condor, globalMode = args.global_mode, nCores = args.n_cores)
    manager.run(args.dry_run)
            
if __name__=="__main__":
    main()
