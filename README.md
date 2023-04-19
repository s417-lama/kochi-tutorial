# Kochi Tutorial

This repository is a tutorial for the Kochi workflow management tool.

Kochi: https://github.com/s417-lama/kochi

Kochi assumes that the project for running experiments is managed as a git project.
In the following tutorial, we use this repository `kochi-tutorial` as the concerning project.

## Run Locally

This section explains how to run Kochi on a local computer without using login or compute nodes.

### Setup

Install Kochi to the local computer:
```sh
pip3 install git+https://github.com/s417-lama/kochi.git
```

The default workspace for Kochi is created at `~/.kochi`.
You can change this location by setting the `KOCHI_ROOT` environmental variable.

To follow this tutorial, clone this repository:
```sh
git clone https://github.com/s417-lama/kochi-tutorial
cd kochi-tutorial/
```

### Spawn Workers

In Kochi, workers must be created to run jobs.

To spawn a worker on the local computer, run:
```sh
kochi work -m local -q test -b
```

Options:
- `-m`: machine name (`local` is a special machine name for local execution)
- `-q`: job queue name
- `-b`: blocking or not

By running the above command, a new job queue `test` is created, and the new worker works on the jobs submitted to the queue `test`.
Without the `-b` option, the worker immediately exits when no job is found in the queue.

The `-m` option is mandatory, but you can omit it by setting the default machine to the local computer by setting:
```sh
export KOCHI_DEFAULT_MACHINE=local
```

Omitting `-m local` option:
```sh
kochi work -q test -b
```

(In the following, we omit the `-m` option.)

You will see the following output by running the above command:
```
Kochi worker 0 started on machine local.
================================================================================
```

You can even spawn multiple workers that work on the same job queue.
Jobs are atomically popped from the job queue (using `flock`).

### Submit Jobs

In Kochi, jobs are first submitted to job queues, and then they are popped by the workers and executed.

To submit a job to the queue named `test`, run the following command by opening a new terminal without closing the worker terminal:
```sh
kochi enqueue -q test -n test_job -- echo foo
```

Then, you will see that the job `echo foo` is executed by the worker spawned above:
```
Kochi job test_job (ID=0) started.
--------------------------------------------------------------------------------
foo
```

You can specify a job name by the `-n` option but not required to do so.

By specifying the `-c` option, you can create a copy of the current git repository (`kochi-tutorial`) to the worker's workspace.

To check it, run:
```sh
kochi enqueue -q test -n test_job -c -- bash -c "pwd && ls"
```

You will see that the command is executed in a different location with a copy of the current workspace:
```
Kochi job test_job (ID=1) started.
--------------------------------------------------------------------------------
$KOCHI_ROOT/workers/local/workspace_0/kochi-tutorial
fib.cpp
fib.yaml
kochi.yaml
README.md
run_fib.bash
```

Uncommitted changes in the current workspace are also copied to the worker's workspace.
However, the changes at the worker's workspace are not reflected to the original workspace.

To check it, run:
```sh
echo hello > foo
kochi enqueue -q test -n test_job -c -- bash -c "echo world >> foo && cat foo"
```

Then, you will see:
```
Kochi job test_job (ID=2) started.
--------------------------------------------------------------------------------
hello
world
```

After the job execution, run locally:
```console
$ cat foo
hello
```

It shows that the job execution is isolated from the original workspace.

Note that the current workspace's state is packed into a job at the job submission time, and later changes to the current workspace are not reflected to the job execution.
This is particularly useful when you are checking the performance results while actively modifying the code, through trial and error.

### Submit Interactive Jobs

You can submit an interactive job to the queue `test` by running:
```sh
kochi interact -q test -c
```

This will launch a shell on the worker's workspace and connect the local terminal to it.
This feature is particularly useful when the worker is launched on a compute node.

### Check Job Status

To list currently enqueued jobs:
```sh
kochi show jobs
```

To include terminated jobs:
```sh
kochi show jobs -t
```

To increase the maximum number of jobs shown:
```sh
kochi show jobs -t -l 200
```

To show job execution status:
```sh
kochi show job <job_id>
```

To show the job output:
```sh
kochi show log job <job_id>
```

To cancel a job:
```sh
kochi cancel <job_id>
```

To cancel all jobs:
```sh
kochi cancel -a
```

### Manage Dependencies

The project-specific configuration is managed in the `kochi.yaml` file.

In this tutorial, we will install [MassiveThreads](https://github.com/massivethreads/massivethreads) as a dependency of this project and run a task-parallel program built on top of MassiveThreads.

Each project dependency is identified by its name and *recipe*.

For example, the configuration of a dependency named `massivethreads` looks like:
```yaml
dependencies:
  massivethreads:
    git: git@github.com:massivethreads/massivethreads.git
    recipes:
      - name: release
        branch: master
        envs:
          CFLAGS: -O3 -Wall
    script:
      - gcc --version
      - ./configure --prefix=$KOCHI_INSTALL_PREFIX --disable-myth-ld --disable-myth-dl
      - make -j
      - make install
```

Settings:
- `git`: URL for the git repository
- `recipes`: the list of recipes
- `script`: shell script to install the dependency to the `$KOCHI_INSTALL_PREFIX` dir

The settings in each recipe will overwrite the settings in the parent level.

To install the `release` recipe, run:
```sh
kochi install -d massivethreads:release
```

The format for the dependency (`-d`) is `<dependency_name>:<recipe_name>`.

To check installation:
```console
$ kochi show installs
Dependency      Recipe    State          Installed Time
--------------  --------  -------------  --------------------------
massivethreads  release   installed      2023-04-18 17:35:43.563509
massivethreads  debug     NOT installed
massivethreads  develop   NOT installed
jemalloc        v5.2.1    NOT installed
```

Let's install another recipe (`debug`), which is configured with `-O0` CFLAGS:
```sh
kochi install -d massivethreads:debug
```

If you want to make some changes to the MassiveThreads code to check its behaviour (e.g., printf debugging), you can *mirror* the local directory status to the installation of the dependency.

For example, suppose that you clone MassiveThreads to the `../massivethreads` dir and make some changes to the local code.
Then, you can use that state of the project as a dependency to this `kochi-tutorial` project, by setting a recipe as follows:
```yaml
recipes:
  - name: develop
    mirror: true
    mirror_dir: ../massivethreads
```

Note that you do not need to commit your changes to git in the `../massivethreads` dir.
The diff from the current HEAD will be copied and applied to the dependency build workspace.

If you install the `massivethreads:develop` recipe and specify it as a dependency for a job, the modified version of the code will run.
This feature is useful for debugging runtime systems or libraries.

Dependencies are not necessarily a git repository.
For example, you can install `jemalloc` from a web tarball:
```sh
kochi install -d jemalloc:v5.2.1
```

See `kochi.yaml` for the specific configuration.

### Run Jobs with Dependencies

To run a job with the dependency `massivethreads:release`, run:
```sh
kochi enqueue -q test -d massivethreads:release -c ./run_fib.bash
```

`run_fib.sh` will compile the `fib.cpp` program and run it.

The installation path to the dependency is passed to the job as an environmental variable `$KOCHI_INSTALL_PREFIX_MASSIVETHREADS`.

The output will look like:
```
Kochi job ANON (ID=3) started.
--------------------------------------------------------------------------------
KOCHI_INSTALL_PREFIX_MASSIVETHREADS=$KOCHI_ROOT/projects/kochi-tutorial/install/local/massivethreads/release
fib(35) = 9227465
Execution Time: 151809458 ns
```

By specifying the `debug` recipe to the job submission:
```sh
kochi enqueue -q test -d massivethreads:debug -c ./run_fib.bash
```

You will get much longer execution time (because of the `-O0` build):
```
Kochi job ANON (ID=4) started.
--------------------------------------------------------------------------------
KOCHI_INSTALL_PREFIX_MASSIVETHREADS=$KOCHI_ROOT/projects/kochi-tutorial/install/local/massivethreads/debug
fib(35) = 9227465
Execution Time: 352041108 ns
```

Additionally, you can specify jemalloc as the dependency:
```sh
kochi enqueue -q test -d massivethreads:release -d jemalloc:v5.2.1 -c ./run_fib.bash
```

The `activate_script` in `kochi.yaml` is executed when the dependency is loaded.
In the case of jemalloc, `LD_PRELOAD` env val is set in this field.

### Setup Job Config Files

The above examples use a shell command or shell script for the job execution, but you can manage your jobs in a more structured way.

`fib.yaml` is an example config file for executing `fib.cpp`.

Fields in the job config file:
- `depends`: specify dependencies and default recipes on which the benchmark depends
- `default_params`: parameter names and their default values
- `default_name`: default job name (which can be overwritten by `-n` option)
- `default_queue`: default queue name (which can be overwritten by `-q` option)
- `default_duplicates`: default number of job duplication for batch job submission (explained later)
- `build`: how to build the benchmark
- `run`: how to run the benchmark

To execute the benchmark with the default configuration, run:
```sh
kochi enqueue -q test fib.yaml
```

Passing a yaml config file implys the `-c` option.

Then, you will see:
```
Kochi job fib (ID=5) started.
--------------------------------------------------------------------------------
Compiling fib...
fib(35) = 9227465
Execution Time: 149035659 ns
```

You can overwrite the default parameters when submitting a job.
Let's change `n_input` and `n_workers`:
```sh
kochi enqueue -q test fib.yaml n_input=38 n_workers=1
```

Then, `fib(38)` will be calculated by spawning only one MassiveThreads worker:
```
Kochi job fib (ID=6) started.
--------------------------------------------------------------------------------
fib(38) = 39088169
Execution Time: 5052224989 ns
```

The parameters are passed to the job script as `$KOCHI_PARAM_<param_name>` environmental variables.

If you increase the number of MassiveThreads workers:
```sh
kochi enqueue -q test fib.yaml n_input=38 n_workers=4
```

You will see a speedup if the machine has multiple cores:
```
Kochi job fib (ID=7) started.
--------------------------------------------------------------------------------
fib(38) = 39088169
Execution Time: 1183553073 ns
```

You may notice that the compilation is skipped for the second and later job executions.
This is because of the `depend_params` field in `fib.yaml`, which indicate which phase (build or run) depends on which parameters.
Only when `depend_params` in the build phase are changed from the last execution (or the current workspace is modified), the benchmark will be compiled again.

To check this behaviour, specify `debug=1` param to the job submission:
```sh
kochi enqueue -q test fib.yaml debug=1
```

You will see that the benchmark is compiled again:
```
Kochi job fib (ID=8) started.
--------------------------------------------------------------------------------
Compiling fib...
fib(35) = 9227465
Execution Time: 296137883 ns
```

You can also overwrite the dependency recipe. For example:
```sh
kochi enqueue -q test fib.yaml debug=1 -d massivethreads:debug
```

You may want to combine it with the interactive job submission:
```sh
kochi interact -q test fib.yaml
```

This will execute the benchmark on the local shell, allowing for further interaction (e.g., running `gdb` for the compiled `fib` executable).

### Submit Batch Jobs

Kochi provides a mechanism to submit multiple jobs with different parameters at a time and record the job execution results.
This is called a batch job.

`fib.yaml` includes a batch job configuration named `batch_test`, which this tutorial will execute.

Fields for batch job config:
- `name`: overwrite the default job name `default_name`
- `queue`: overwrite the default queuename `default_name`
- `duplicates`: overwrite the default number of job duplication `default_duplicates`. This number of jobs with the same parameters will be redundantly submitted to the queue.
- `params`: overwrite the default parameters. If a list of values is given, then all combinations of them will be submitted as jobs.
- `artifacts`: specify how to save the computational artifacts (e.g., job output and stats)

By default, all changes to the worker's workspace are discarded, but you can save the job output and files by explicitly specifying them as the job artifacts.

The types of the artifacts can be:
- `stdout`: a job's standard output is saved as a file at path `dest`
- `stats`: a job's configuration and execution status are saved as a file at path `dest`
- `file`: when a job is finished, the file in the current workspace at path `src` is copied to the artifact path `dest`

Please be careful not to set the same file name for different jobs, as they can be overwritten by another job artifact.

You can use the following special substitutions to set different file names for each parameter etc.:
- `${batch_name}` will be substituted with the batch name (`batch_test` in this case).
- `${<param>}` will be substituted with the parameter value
- `${duplicate}` will be substituted with the index (0, 1, 2, ...) for the duplicated jobs

Before submitting your first batch job, you must create a new git branch `kochi_artifacts` and a separate git worktree dir for it.

The following command automatically creates a git worktree in the parent directory of `kochi-tutorial`:
```sh
kochi artifact init ../kochi-tutorial-artifact
```

The artifacts of the batch job are pushed into a separate git branch.
The above command creates an orphan git branch for it (`kochi_artifacts`).

Then, you can submit a batch job:
```sh
kochi batch fib.yaml batch_test
```

You will see multiple jobs are submitted to the queue `fib_batch_test`.
Since `batch_test` has two `n_input` values, three `n_workers` values, and three job duplications, it launches 18 jobs in total.

To execute these jobs, you need to launch a new worker working on the queue:
```sh
kochi work -q fib_batch_test
```

After all jobs are finished, the worker will automatically exit.

Now all artifacts shoule be saved in Kochi, so let's pull the artifacts:
```sh
cd ../kochi-tutorial-artifact/
kochi artifact sync -m local
```

Note that you need to move to the artifact worktree dir first.
You will see that the `local/fib/` directory in the `kochi-tutorial-artifact` branch contains the log output and stats.

## Run Remotely

This section explains how to run jobs on remote compute nodes via a login node from the local computer.

The basics of job management is the same as the local execution, but some extra machine settings are needed.

### Setup

Install Kochi to the local computer, login node, and compute nodes:
```sh
pip3 install git+https://github.com/s417-lama/kochi.git
```

Kochi assumes that login nodes and compute nodes all share a shared file system (NFS).
If not, Kochi cannot be used for that machine.

If the home directory is not shared but some directories are shared, then set `KOCHI_ROOT` to within the shared directories.

This `kochi-tutorial` project is not needed to be cloned on the remote nodes.

### Machine Configuration

`kochi.yaml` contains a machine configuration (`spica` machine), but it is specific to my environment and should be modified.

Fields for machine configuration:
- `login_host`: host name of the login node. This node should be able to be connected by the command `ssh <login_host>`. If not, please configure `~/.ssh/config` accordingly.
- `work_dir` (optional): working directory (`$HOME` by default)
- `kochi_root` (optional): `KOCHI_ROOT` on remote servers (`$HOME/.kochi` by default)
- `alloc_interact_script` (optional): how to submit an interactive job to the system's job manager
- `alloc_script` (optional): how to submit a noninteractive job to the system's job manager
- `load_env_script` (optional): scripts to be first executed on remote servers. This can be separately set for a login node (`on_login_node`) and compute nodes (`on_machine`).

The following is a template for slurm:
```yaml
machines:
  <machine_name>:
    login_host: <login_host>
    alloc_interact_script: srun -w <compute_node_name> --pty bash
    alloc_script: sbatch --wrap="$KOCHI_WORKER_LAUNCH_CMD" -w <compute_node_name> -o /dev/null -e /dev/null -t ${KOCHI_ALLOC_TIME_LIMIT:-0}
```

Please substitute the following to match your system configuration:
- `<machine_name>` is an arbitrary name to identify the machine
- `<login_host>`: hostname of the login node that can be connected from the local computer by the command `ssh <login_host>`
- `<compute_node_name>`: the name of compute nodes managed by slurm

The following environmental variables are passed to the allocation scripts:
- `KOCHI_WORKER_LAUNCH_CMD`: scripts to launch a Kochi worker on a compute node
- `KOCHI_ALLOC_TIME_LIMIT`: time limit that should be passed to the system's job manager, which is specified by the `-t` option of the `kochi alloc` command explained later
- `KOCHI_ALLOC_NODE_SPEC`: specification of the nodes (e.g., number of nodes for MPI) allocated by the system's job manager, which is specified by the `-n` option of the `kochi alloc` command explained later

As Kochi frequently accesses the login node via ssh, it is recommended to set the following configuration in `~/.ssh/config`:
```
Host <login_host>
  ...
  ControlPath ~/.ssh/mux-%r@%h:%p
  ControlMaster auto
  ControlPersist yes
```

This enables ssh multiplexing, which persists the connection shared by multiple concurrent ssh sessions.

### Spawn Workers on Compute Nodes

If you specify the `alloc_interact_script` to the machine configuration, you can easily allocate an interactive job on a compute node by running the following command on the local computer:
```sh
kochi alloc_interact -m <machine_name> -q test
```

This will login to the `<login_node>` via ssh, and execute `alloc_interact_script` on the login node.
The commands to launch a Kochi worker are automatically sent to the interactive shell.

If a Kochi worker started on a compute node, it succeeded.
You can submit a job from the local computer:
```sh
kochi enqueue -m <machine_name> -q test -- echo foo
```

Then `foo` will be output in the shell on the compute node.
Of course, you can omit `-m <machine_name>` by setting `export KOCHI_DEFAULT_MACHINE=<machine_name>`.

To allocate noninteractive jobs:
```sh
kochi alloc -m <machine_name> -q test -f
```

Specify the `-f` option to follow the standard output generated by the worker.

The above command will immediately exit because the job queue `test` has no job.
Please try submitting some jobs before the worker launch and allocate the worker again.

To see more options:
```sh
kochi alloc --help
```

You can also check the output of a specific worker:
```sh
kochi show log worker <worker_id>
```

Note that even if you do not provide `alloc_interact_script` or `alloc_script`, you can manually launch workers by the command `kochi work -m <machine_name> -q <queue_name>` on compute nodes.

### Submit Batch Jobs

Kochi is designed to deal with experimental results generated on multiple machines and handle them in a single repository.

In the `kochi_artifacts` branch, batch job artifacts are saved in the `<machine_name>` directory separately for each machine.

Internally, a artifact branch (`kochi_artifacts_<machine_name>`) is created for each machine and all artifacts are first pushed into the branch.
Then, when `kochi artifact sync -m <machine_name>` is executed, the `kochi_artifacts_<machine_name>` branch is merged into the main `kochi_artifacts` branch.
Conflict is very unlikely because the changes are usually made in each `<machine_name>` directory individually.

### Inspect Compute Nodes

You can login to the compute node where a worker is running by executing the following command:
```sh
kochi inspect -m <machine_name> <worker_id>
```

A Kochi worker launches a user-level ssh daemon at the worker startup time, so you can login to the compute node while the worker is running.
This feature is useful when interactive job submission is not allowed by the system's job manager but you want to run interactive commands on the compute nodes.

Some use cases are:
- Watch the machine stats during job execution (e.g., `top`, `free` command)
- Attach gdb to the executing program when a job gets stuck

You can also submit an interactive job by the command `kochi interact` to operate on compute nodes.
This is different from `kochi inspect` in that `kochi interact` submits a job, which is executed as a child shell process of the worker's process, while `kochi inspect` logins to compute nodes as a separate ssh session.
Thus, for example, if you want to debug a program in the current workspace on a compute node, `kochi interact` is recommended.
