#!/bin/sh
#this batch script request 4 nodes, 36 cores total
#more info requesting specific nodes see
#"man pbs_resources"
#PBS -V
#PBS -l nodes=2:ppn=44:core44
#PBS -N parconnect
#PBS -joe
#PBS -q batch
cd $PBS_O_WORKDIR
NODE=`hostname`
NCORES=`wc -w < $PBS_NODEFILE`
DATE=`date`
echo "job submitted: $DATE on $NODE"

#perf-report /usr/local/openmpi-gcc/bin/mpirun --mca btl openib,self,sm --nooversubscribe -np $NCORES --map-by node -machinefile $PBS_NODEFILE ./bin/benchmark_parconnect --input dbg --file input/20,000,000.fastq &> output.$PBS_JOBID
#perf-report /usr/local/openmpi-gcc/bin/mpirun --mca btl openib,self,sm --nooversubscribe -np $NCORES --map-by node -machinefile $PBS_NODEFILE ./bin/benchmark_parconnect --input dbg --file input/40,000,000.fastq &> output.$PBS_JOBID
#perf-report /usr/local/openmpi-gcc/bin/mpirun --mca btl openib,self,sm --nooversubscribe -np $NCORES --map-by node -machinefile $PBS_NODEFILE ./bin/benchmark_parconnect --input dbg --file input/100,000,000.fastq &> output.$PBS_JOBID
perf-report /usr/local/openmpi-gcc/bin/mpirun --mca btl openib,self,sm --nooversubscribe -np $NCORES --map-by node -machinefile $PBS_NODEFILE ./bin/benchmark_parconnect --input dbg --file input/649.4.815.fastq &> output.$PBS_JOBID

