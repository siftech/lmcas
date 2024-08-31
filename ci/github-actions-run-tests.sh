#!/usr/bin/env bash
set -euxo pipefail

results_dir="./result"
mkdir -p "$results_dir/test-results/logs"

testing_dir="test/tabacco-targets"
testResults="$results_dir/test-results"

echo $DOCKER_CONTAINER_REGISTRY_TOKEN | docker login ghcr.io -u mzhang-sift --password-stdin

runTest() {
  target="$1"
  log_file="$2"
  bin_dir="$3"
  test_results="$4"

  dst_results="/test/$target/test-results"

  read -ra script <<< "$5 $dst_results/logs/$log_file"
  errorMessage="Failed to produce log output for target $target"

  mkdir -p "$test_results" 

  # sh "${script[@]}" || [ -f "$test_results/logs/$log_file" ]  || (echo "$errorMessage" && exit 1)
  ctrName="test-$target"
  docker run \
    --rm \
    --name="$ctrName" \
    --mount type=bind,src="$(realpath test/tabacco-targets)",dst=/test/tabacco-targets \
    --mount type=bind,src="$(realpath "$test_results")",dst="$dst_results" \
    --mount type=bind,src="$(realpath "$bin_dir")",dst=/test/"$target"/target-binaries \
    --mount type=bind,src="$(realpath utils)",dst=/test/utils \
    ghcr.io/siftech/lmcas-test:test \
    sh "${script[@]}" || [ -f "$test_results/logs/$log_file" ]  || (echo "$errorMessage" && exit 1)
}

if [ "$(ls -A "$results_dir"/target-binaries/debloated-targets/)" ]; then
  for dir in "$results_dir"/target-binaries/debloated-targets/*/; do
    target=$(basename "$dir")
    test_target_dir=$testing_dir/$target

    for test_dir in "$test_target_dir"/*/; do
      if [ -f "$test_dir/test.sh" ]; then
        test_name=$(basename "$test_dir")
        log_base="$target-$test_name"

        if [ "$target" = "tabacco-cps" ] || [ "$target" = "uwisc" ]; then
          log_file="$log_base-log.json"
          run_tests="test/tabacco-targets/$target/$test_name/test.sh"
          runTest "$target" "$log_file" "$dir" "$testResults" "$run_tests"
        else
          bloated_log="$log_base-bloated-log.json"
          bloated="test/tabacco-targets/$target/$test_name/test.sh bloated"		 
          debloated_log="$log_base-debloated-log.json"
          debloated="test/tabacco-targets/$target/$test_name/test.sh debloated"
          runTest "$target" "$bloated_log" "$dir" "$testResults" "$bloated"
          runTest "$target" "$debloated_log" "$dir" "$testResults" "$debloated"
        fi
      fi
    done
  done
fi