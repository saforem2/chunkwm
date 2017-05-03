#!/bin/bash
id=$(chunkc query _last_active_desktop)
chunkc window -d $id; khd -p "cmd + alt - $id"
