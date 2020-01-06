import yaml

class TaskWebpage(object):
    def __init__(self):
        ##Header of worflow HTML
        self._htmlHead = """
            <!DOCTYPE html>
            <html>
                <head>
                    <style>
                        html, body {height : 100%}

                        div {font-size: 70%; position:absolute; border-radius: 10px; border-style: solid; border-width: 1px; box-shadow: 5px 5px 2px grey;}
                        div.task {text-align: center; width: 250px; height: 28px;}
                        div.yaml {padding:10px; display: none; top: 0px; left: 0px; width: auto; height: auto; z-index: 1; background-color: white}
                        
                        svg {width: 100%; height: 100%; top:0; left:0; position: absolute;}
                        object {width: 200px; height: 1000px;}

                    </style>
                    <script type="text/javascript" src="http://livejs.com/live.js"></script>

                </head>
            """

        ##Body of worflow HTML
        self._htmlBody = """
                    <div class='yaml' onmouseover='showYaml(this)' onmouseout='hideYaml()'></div>

                    <svg>
                        {lines}
                    </svg>

                    {divs}

                    <script>
                        document.getElementsByTagName('svg')[0].style.height = '{maxheight}px'
        """

        ##Javascript functions of worflow HTML
        self._javascript = """
                    function showYaml(x){
                        var divYaml = document.getElementsByClassName("yaml")[0];
                        divYaml.style.display = 'block';

                        if(x.className != "yaml"){
                            divYaml.style.top = x.style.top;
                            divYaml.style.left = parseInt(x.style.left) + 260 + "px";
                            divYaml.innerHTML = x.getAttribute("data-yaml");
                        }
                    }

                    function hideYaml(){
                        var divYaml = document.getElementsByClassName('yaml')[0];
                        divYaml.style.display = 'none';
                    }
                    </script>
                </body>
            </html>
            """

        ##Templates for divs and lines for the task and their dependencies
        self._divTmp = "<div class='task' style='left:{x}px; top:{y}px; background:{color};' onmouseover='showYaml(this)' data-yaml='{yaml}'> {name} </div>"
        self._lineTmp = "<line x1='{x1}px' y1='{y1}px' x2='{x2}px' y2='{y2}px' stroke='gray'></line>"


    ##Wrap all keys in block to make them colored
    def __dicToNiceHtml(self, dic):
        newDic = {}
        for (key, value) in dic.items():
            if type(value) != dict:
                newDic["<b style='color:brown'>{}</b>".format(key)] = value

            else:
                newDic["<b style='color:brown'>{}</b>".format(key)] = dicToNiceHtml(value)

        return newDic

    def createWebpage(self, graph):
        divPos = {}
        maxheight = 0.

        divs = []
        yamls = []
        lines = []

        statusToColor = {
                        "VALID": "linear-gradient(to bottom right, #f4ff00, #e8fb15, #dcf821, #d1f42a, #c6f032)",
                        "SUBMITTED": "linear-gradient(to bottom right, #ff8500, #f38900, #e68c00, #da8f00, #cf9106)",
                        "RUNNING": "linear-gradient(to bottom right, #00ccff 0%, #3333cc 111%)",
                        "FINISHED": "linear-gradient(to bottom right, #00ff00 0%, #009933 111%)",
                        "FAILED": "linear-gradient(to bottom right, #ff0000 0%, #cc0066 111%)",
        }

        for (layerIndex, taskLayer) in enumerate(graph):
            for index, task in enumerate(taskLayer):
                ##Translate task config into yaml like string
                yamlInfo = yaml.dump(self.__dicToNiceHtml(task), default_flow_style=False, width=float("inf"), indent=4).replace("\n", "<br>").replace("\'", "").replace("    ", "&emsp;&emsp;")

                divPos[task["name"]] = (task["dependencies"], yamlInfo, task["status"], task["display-name"], layerIndex*400, index*40)

                if index*40 + 40 > maxheight:
                    maxheight = index*40 + 40

        ##Fill div/lines templates with right information of the task into list
        for taskName, (dependencies, yamlInfo, status, display, x, y) in divPos.items():
            divs.append(self._divTmp.format(yaml=yamlInfo, x=x, y=y, name = display, color=statusToColor[status]))

            if len(dependencies) != 0:
                for dependency in dependencies: 
                    lines.append(self._lineTmp.format(x1=x, y1=y+12.5, x2=divPos[dependency][4]+250, y2=divPos[dependency][5]+12.5))

        ##Write workflow html
        with open("workflow.html", "w") as f:
            f.write(self._htmlHead + self._htmlBody.format(divs="\n".join(divs), lines = "\n".join(lines),maxheight=maxheight) + self._javascript)
