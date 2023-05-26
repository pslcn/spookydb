#!/bin/sh

# Count number of lines in $(git ls-files) using `wc`

loc=0

files=$(git ls-files)

for file in $files
do
	lines=$(wc -l $file | cut -f 1 -d ' ')
	loc=$(($loc + $lines))
done

echo $loc LOC
