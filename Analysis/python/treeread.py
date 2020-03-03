from task import Task
import utils

import os
import yaml

class TreeRead(Task):
    def __init__(self, config = {}):
        super().__init__(config)

    def run(self):
        self["executable"] = "treeread"

        self["arguments"] = [
                "--parameters", *self["parameters"],
                "--cuts", *self["cuts"], 
                "--out-name", self["output"],  
                "--channel", self["channel"],    
                "--filename", self["filename"], 
                "--clean-jet", self["clean-jet"], 
                "--event-yield", *self["interval"]
        ]
        
    def output(self):
        self["output"] = "{}/{}.root".format(self["dir"], self["name"])

    @staticmethod
    def configure(config, channel, prefix=""):
        ##Dic with process:filenames 
        processDic = yaml.load(open("{}/ChargedAnalysis/Analysis/data/process.yaml".format(os.environ["CHDIR"]), "r"), Loader=yaml.Loader)

        skimDir = os.environ["CHDIR"] + "/OldSkim"
        tasks = []
 
        for process in config["processes"] + config["data"].get(channel, []):
            ##List of filenames for each process
            filenames = ["{skim}/{file}/merged/{file}.root".format(skim=skimDir, file = f) for f in processDic[process]]

            for i, filename in enumerate(filenames):
                intervals = utils.SplitEventRange(filename, channel, config["number-events"])

                for j, interval in enumerate(intervals):
                    ##Configuration for treeread Task
                    task = {
                            "name": "{}_{}_{}".format(channel, process, i+j) + ("_{}".format(prefix) if prefix else ""), 
                            "display-name": "Hist: {} ({})".format(process, channel),
                            "channel": channel, 
                            "cuts": config["cuts"].get("all", []) + config["cuts"].get(channel, []),
                            "dir":  os.environ["CHDIR"] + "/{}/{}/{}/unmerged/{}".format(config["dir"], config["chan-dir"][channel], process, i+j), 
                            "process": process, 
                            "parameters": config["parameters"].get("all", []) + config["parameters"].get(channel, []),
                            "filename": filename,
                            "interval": interval,
                            "run-mode": config["run-mode"],
                            "clean-jet": config.get("clean-jet", {}).get(channel, "")
                    }

                    tasks.append(TreeRead(task))

        return tasks
