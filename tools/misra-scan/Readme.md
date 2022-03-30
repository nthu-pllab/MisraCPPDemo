# misra-scan


## 1. Dependency

### 1.1 Python version
* Python >= 3.6.4

### 1.2 Python packages
* Jinja2 >= 2.10
* python-dateutil >= 2.6.1

### 1.3 install packages
Please make sure Python3 is installed on your system.
You can use one of the following command to install required packages.
```
$ pip3 install --user -r requirements.txt
$ python3 -m pip install --user -r requirements.txt
```
If pip is not installed, please check this [link](https://packaging.python.org/guides/installing-using-linux-tools/#installing-pip-setuptools-wheel-with-linux-package-managers).


## 2. Config

### 2.1 default config
```config.json``` is the default config for **misra-scan**.
The following example shows how the config looks like.
```
{
    "name": "MisraC 2012 Config Example",
    "scanbuild_path": "/home/jpdyuki/llvm/build/bin/scan-build",
    "misrac_so_path": "/home/jpdyuki/llvm/build/lib/MisracppChecker.so",
    "checkers": [
        "MisraC.7_1",
        "MisraC.10_1"
    ]
}
```
As you can see, the content should be a valid JSON string.
Before running **misra-scan** for the first time, you have to set the values for "scanbuild_path" and "misrac_so_path" in ```config.json``` by yourself.
All MisraC checkers are disabled by default. To enable the checkers, please specify the checkers at "checkers" section in config.

### 2.2 customized config
In addition to default config, providing a customized config is supported. (See 'Usage' section.) For the convenience, you don't have to specify the values for "scanbuild_path" and "misrac_so_path" in customized config. For example, the following example is a valid customized config.
```
{
    "name": "Customized Config Example",
    "checkers": [
        "MisraC.7_1",
        "MisraC.10_1"
    ]
}
```


## 3. Usage

```
$ misra-scan [options] <build command>
```
Before invoking **misra-scan** for analyzing your project, please change the current working directory to root of your project, and make sure there is a Makefile.

### 3.1 -o option
This option specifies the output directory for analyzer reports.
If it is not specified, default output directory will be set as "./report"

### 3.2 -config-path option
This option specifies customized config path. Notice that the customized config can be put under any other directories, but the filename should be "config.json" only.

### 3.3 example
```
$ misra-scan -o ../report make  # invoke misra-scan with default config, and the reports will be generated at the directory '../report'
$ misra-scan -o ../report -config-path ../config.json make  # invoke misra-scan with customized config '../config.json', and the reports will be generated at the directory '../report'
$ misra-scan make  # invoke misra-scan with default config, and the reports will be generated at the directory './report'
```
