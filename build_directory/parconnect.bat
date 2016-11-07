#!/bin/sh
#this batch script request 4 nodes, 36 cores total
#more info requesting specific nodes see
#"man pbs_resources"
#PBS -V
#PBS -l nodes=node1:ppn=2
#PBS -N parconnect
#PBS -joe
#PBS -q batch
cd $PBS_O_WORKDIR
NODE=`hostname`
NCORES=`wc -w < $PBS_NODEFILE`
DATE=`date`
echo "job submitted: $DATE on $NODE"
#mpirun -np $NCORES --map-by node -machinefile $PBS_NODEFILE ./bin/benchmark_parconnect --input dbg --file 649.4.815.fastq &> output.$PBS_JOBID
mpirun -np $NCORES --map-by node -machinefile $PBS_NODEFILE ./bin/benchmark_parconnect --input dbg --file small.fastq &> output.$PBS_JOBID
#mpirun -np $NCORES --map-by node -machinefile $PBS_NODEFILE ./bin/benchmark_parconnect --input dbg --file tiny.fastq &> output.$PBS_JOBID
