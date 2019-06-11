#!/bin/bash
git checkout --orphan new-master
git add .
git commit -m "$1"
git branch -m master old-master
git branch -m new-master master
git push --force --set-upstream origin master
git branch -D old-master
git push
