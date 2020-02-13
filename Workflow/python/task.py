from abc import ABC, abstractmethod
import yaml
import os
import shutil
import subprocess

class Task(ABC, dict):
    def __init__(self, config = {}):
        ##Default values
        self["name"] = "Task_{}".format(id(self))
        self["status"] = "VALID"
        self["dir"] = os.getcwd() + "/"
        self["dependencies"] = []
        self["run-mode"] = "Local"

        ##Update with input config
        self.update(config)

        if "display-name" not in self:
            self["display-name"] = self["name"]

    def __hash__(self):
        return id(self)

    def __call__(self):
        if self["run-mode"] == "Local":
            return self.runJob()

        if self["run-mode"] == "Condor":
            return self.runScript()

    def runJob(self):
        logs = []
        errors = []

        if self["run-mode"] == "Local":
            result = subprocess.run([self["executable"], *[str(s) for s in self["arguments"]]], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

            if result.returncode != 0:
                for line in result.stderr.decode('utf-8').split("\n"):
                    errors.append(line)

                return errors

            for line in result.stdout.decode('utf-8').split("\n"):
                logs.append(line)

            return logs

    def runScript(self):
        ##Create directory with condor
        self["condor-dir"] = "{}/Condor/{}".format(self["dir"], self["name"])
        shutil.rmtree(self["condor-dir"], ignore_errors=True)
        os.makedirs(self["condor-dir"], exist_ok=True)

        ##Write executable
        fileContent = [
                        "#!/bin/bash\n", 
                        "cd $CHDIR\n",
                        "source ChargedAnalysis/setenv.sh Analysis\n",
                        " ".join([self["executable"], *[str(s) for s in self["arguments"]]])
        ]

        with open("{}/run.sh".format(self["condor-dir"]), "w") as condExe:
            for line in fileContent:
                condExe.write(line)

        result = subprocess.run(["chmod", "a+x", "{}/run.sh".format(self["condor-dir"])], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        return result.stdout.decode('utf-8').split("\n")

    def getDependentFiles(self, depGraph):
        if self["tasklayer"] == 0:
            return 0

        ##Loop over all dependecies and save information
        for task in depGraph[self["tasklayer"]-1]:
            if task["name"] in self["dependencies"] and type(task["output"]) == str:
                if len(task["output"]) != 0:
                    self.setdefault("dependent-files", []).append(task["output"])

    def dump(self):
        ##Dump this task config in yaml file
        with open("{}/{}.yaml".format(self["dir"], self["name"]), "w") as configFile:
            yaml.dump(dict(self), configFile, default_flow_style=False, width=float("inf"), indent=4)

    def createDir(self):
        ##Create directory of this task
        os.system("mkdir -p {}".format(self["dir"]))

    def checkOutput(self):
        ##Check if output already exists
        if type(self["output"]) == str:
            return os.path.exists(self["output"])

        else:
            for output in self["output"]:
                if not os.path.exists(output):
                    return False

            return True                
        
    ##Abstract functions which has to be overwritten
    @abstractmethod
    def run(self):
        pass

    @abstractmethod
    def output(self):
        pass
