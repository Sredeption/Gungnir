#!/usr/bin/env bash
rsync -a -e "ssh -p 3022" --exclude 'cmake-build-*' /home/issac/Workspace/DistributedSystem/Gungnir hkucs@localhost:/home/hkucs/Migration
