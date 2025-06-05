#!/bin/bash
lora="$1"
lora_name=$(strings "${lora}" | head -n 1 | jq .__metadata__.ss_output_name | sed -e 's/"//g')
mapfile -t lora_tags < <(strings "${lora}" | head -n 1 | jq .__metadata__.ss_tag_frequency | sed -e 's/\\//g' -e 's/^"//g' -e 's/"$//g' | jq .[] | jq -r 'to_entries | sort_by(-.value) | .[].key')

echo "${lora}" | sed -e 's/.safetensors//g'
echo "${lora_name}"
for tag in "${lora_tags[@]}" ; do
   echo "${tag}"
done
