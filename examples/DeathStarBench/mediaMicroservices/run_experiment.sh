#!/bin/bash

TIMELOOPS_HOME=/users/meghna/timeloops-ebpf
MM_HOME=/users/meghna/timeloops-ebpf/examples/DeathStarBench/mediaMicroservices
LOADGENERATOR_PATH=$MM_HOME/loadgenerator/loadgenerator.yaml

if [ $# -ne 2 ]; then
    echo "Usage: $0 <NUM_USERS> <TRIAL>"
    exit 1
fi

NUM_USERS=$1
TRIAL=$2
EXPERIMENT_NAME=${NUM_USERS}u_mm_compose_10m_3n_exp_l0.02

kubectl apply -f mm-configmap.yaml
mkdir -p $TIMELOOPS_HOME/experiments/data/mediaMicroservices/$EXPERIMENT_NAME/

function sleep_with_message() {
    local sleep_duration=$1
    local elapsed_time=0

    while [ $elapsed_time -lt $sleep_duration ]; do
        if [ $elapsed_time -gt 0 ]; then
            echo "Slept for $((elapsed_time / 60)) minute(s)..."
        fi
        sleep 60
        elapsed_time=$((elapsed_time + 60))
    done

    remaining_time=$((sleep_duration % 60))
    if [ $remaining_time -gt 0 ]; then
        sleep $remaining_time
        elapsed_time=$((elapsed_time + remaining_time))
    fi

    echo "Slept for $((elapsed_time / 60)) minute(s) and $((elapsed_time % 60)) second(s)."
}

function update_users_in_yaml() {
    local yaml_path=$1
    local num_users=$2
    sed -i '/name: USERS/{n;s/value:.*/value: "'$num_users'"/}' $yaml_path
}

function sleep_with_message() {
    local sleep_duration=$1
    local elapsed_time=0

    while [ $elapsed_time -lt $sleep_duration ]; do
        if [ $elapsed_time -gt 0 ]; then
            echo "Slept for $((elapsed_time / 60)) minute(s)..."
        fi
        sleep 60
        elapsed_time=$((elapsed_time + 60))
    done

    remaining_time=$((sleep_duration % 60))
    if [ $remaining_time -gt 0 ]; then
        sleep $remaining_time
        elapsed_time=$((elapsed_time + remaining_time))
    fi

    echo "Slept for $((elapsed_time / 60)) minute(s) and $((elapsed_time % 60)) second(s)."
}

###################### SETUP ######################

sh $TIMELOOPS_HOME/src/clean.sh
kubectl apply -f $MM_HOME/mm-configmap.yaml
mkdir -p $TIMELOOPS_HOME/experiments/data/mediaMicroservices/$EXPERIMENT_NAME/
sleep 30
update_users_in_yaml $LOADGENERATOR_PATH $NUM_USERS

###################### TIMELOOPS ######################
echo "Running timeloops..."
sh $TIMELOOPS_HOME/src/clean.sh
sleep 30 
kubectl apply -f $TIMELOOPS_HOME/src/daemonset-syscalls.yaml
sleep 100

helm install media $MM_HOME/helm-chart/mediamicroservices
sleep 60 
kubectl apply -f $MM_HOME/loadgenerator/loadgenerator.yaml
sleep_with_message 1000
sh $TIMELOOPS_HOME/experiments/data/extract.sh ${EXPERIMENT_NAME}_timeloops_trial_${TRIAL}
mv ${EXPERIMENT_NAME}_timeloops_trial_${TRIAL}.txt $TIMELOOPS_HOME/experiments/data/mediaMicroservices/$EXPERIMENT_NAME/
mv locust_stats_${EXPERIMENT_NAME}_timeloops_trial_${TRIAL}.txt $TIMELOOPS_HOME/experiments/data/mediaMicroservices/$EXPERIMENT_NAME/

###################### UNHARDENED ######################
echo "Running unhardened..."
sh $TIMELOOPS_HOME/src/clean.sh
sleep 30
helm install media $MM_HOME/helm-chart/mediamicroservices
sleep 30 
kubectl apply -f $MM_HOME/loadgenerator/loadgenerator.yaml
sleep_with_message 800
sh $TIMELOOPS_HOME/experiments/data/extract.sh ${EXPERIMENT_NAME}_unhardened_trial_${TRIAL}
mv ${EXPERIMENT_NAME}_unhardened_trial_${TRIAL}.txt $TIMELOOPS_HOME/experiments/data/mediaMicroservices/$EXPERIMENT_NAME/
mv locust_stats_${EXPERIMENT_NAME}_unhardened_trial_${TRIAL}.txt $TIMELOOPS_HOME/experiments/data/mediaMicroservices/$EXPERIMENT_NAME/

###################### HARDENED ######################
echo "Running hardened..."
sh $TIMELOOPS_HOME/src/clean.sh
sleep 10
helm install media $MM_HOME/helm-chart/asan-mediamicroservices
sleep 30 
kubectl apply -f $MM_HOME/loadgenerator/loadgenerator.yaml
sleep_with_message 1200
sh $TIMELOOPS_HOME/experiments/data/extract.sh ${EXPERIMENT_NAME}_hardened_trial_${TRIAL}
mv ${EXPERIMENT_NAME}_hardened_trial_${TRIAL}.txt $TIMELOOPS_HOME/experiments/data/mediaMicroservices/$EXPERIMENT_NAME/
mv locust_stats_${EXPERIMENT_NAME}_hardened_trial_${TRIAL}.txt $TIMELOOPS_HOME/experiments/data/mediaMicroservices/$EXPERIMENT_NAME/

###################### GRAPHING ######################

python3 $TIMELOOPS_HOME/experiments/graph.py --baseline $TIMELOOPS_HOME/experiments/data/mediaMicroservices/$EXPERIMENT_NAME/${EXPERIMENT_NAME}_unhardened_trial_${TRIAL}.txt --hardened $TIMELOOPS_HOME/experiments/data/mediaMicroservices/$EXPERIMENT_NAME/${EXPERIMENT_NAME}_hardened_trial_${TRIAL}.txt --timeloops $TIMELOOPS_HOME/experiments/data/mediaMicroservices/$EXPERIMENT_NAME/${EXPERIMENT_NAME}_timeloops_trial_${TRIAL}.txt --out $TIMELOOPS_HOME/experiments/${EXPERIMENT_NAME}_trial_${TRIAL}.pdf