# C++ project boilerplate

Barebone CMake project for C++ projects.

## Defining the project name

Open `/path/to/repo/CMakeLists.txt` file and edit the fields below with your project name.

```
set(PROJECT_FOLDER "boilerplate")

# Project name.
project(boilerplate)

# Target executable names.
set(MAIN_TARGET "boilerplate")
```

e.g.

```
set(PROJECT_FOLDER "getcrc32map")

# Project name.
project(getcrc32map)

# Target executable names.
set(MAIN_TARGET "getcrc32map")
```

Open `/path/to/repo/build.bat` file and edit `PROJECT_NAME` with your project name.

Must be the same used as `YOUR_PROJECT_NAME` in CMakeLists.txt `project` field.

```
rem Set project name below
set PROJECT_NAME=boilerplate
```

e.g.

```
rem Set project name below
set PROJECT_NAME=getcrc32map
```

## Create the VS project
To create the VS project, run `/path/to/repo/create_project.bat`

## Build the project
To build the project, run `/path/to/repo/build.bat`

## Automatic building
There's a Github action workflow that creates the project and builds it automatically when code is pushed to the repo. See `/path/to/repo/.github/workflows/build.yml`
