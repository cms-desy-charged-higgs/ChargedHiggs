from task import Task

from ROOT import TFile, TTree

import os
import yaml

class TreeRead(Task):
    def __init__(self, config = {}):
        super().__init__(config)

        if not "y-parameter" in self:
            self["y-parameter"] = []

        if not "save-mode" in self:
            self["save-mode"] = "Hist"

    def status(self):
        if os.path.isfile(self["output"]):
            self["status"] = "FINISHED"
        
    def run(self):
        self["executable"] = "TreeRead"

        self["arguments"] = "'{}' '{}' '{}' '{}' '{}' '{}' '{}' '{}' '{}'".format(
                self["process"], 
                " ".join(self["x-parameter"]), 
                " ".join(self["y-parameter"]), 
                " ".join(self["cuts"]), 
                self["output"],  
                self["channel"],    
                self["save-mode"],
                self["filename"], 
                " ".join(self["interval"])
        )

        if self["run-mode"] == "Local":
            os.system("{} {}".format(self["executable"], self["arguments"]))

        if self["run-mode"] == "Condor":
            self.createCondor()
            os.system("condor_submit {}/condor.sub".format(self["condor-dir"]))
        
    def output(self):
        self["output"] = "{}/{}.root".format(self["dir"], self["name"])

    @staticmethod
    def configure(conf, channel):
        nEvents = int(1e6)

        chanToDir = {"mu4j": "Muon4J", "e4j": "Ele4J", "mu2j1f": "Muon2J1F", "e2j1f": "Ele2J1F", "mu2f": "Muon2F", "e2f": "Ele2F"}

        ##Dic with process:filenames 
        processDic = yaml.load(open("{}/ChargedAnalysis/Analysis/data/process.yaml".format(os.environ["CHDIR"]), "r"), Loader=yaml.Loader)

        skimDir = os.environ["CHDIR"] + "/Skim"
        tasks = []

        for process in conf[channel]["processes"]:
            nJobs = 0

            ##List of filenames for each process
            filenames = ["{skim}/{file}/merged/{file}.root".format(skim=skimDir, file = processFile) for processFile in processDic[process]]   

            for filename in filenames:
                rootFile = TFile.Open(filename)
                rootTree = rootFile.Get(channel)
                entries = rootTree.GetEntries()
                rootFile.Close()

                ##Complicated way of calculating intervals
                intervals = [[str(nEvents*i) if i == 0 else str(nEvents*i + 1), str(nEvents + (entries-nEvents)) if i == round(entries/nEvents) else str(nEvents*(i+1))] for i in range(round(entries/nEvents +0.5))]

                for interval in intervals:
                    ##Configuration for treeread Task
                    config = {"name": "Hist_{}_{}_{}".format(channel, process, nJobs), 
                              "display-name": "Hist: {} ({})".format(process, channel),
                              "channel": channel, 
                              "cuts": conf[channel]["cuts"], 
                              "dir":  os.environ["CHDIR"] + "/Hist/{}/{}".format(conf[channel]["dir"], chanToDir[channel]), 
                              "process": process, 
                              "x-parameter": conf[channel]["x-parameter"], 
                              "filename": filename,
                               "interval": interval,
                    }

                    tasks.append(TreeRead(config))
                    nJobs+=1

        return tasks
