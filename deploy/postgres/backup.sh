#!/bin/bash

DB_USER="echolink_user"
DB_NAME="echolink_db"
BACKUP_DIR= "/backup"

CURRENT_DATE=$(date +%Y-%m-%d)

BACKUP_FILE= "$BACKUP_DIR/backup_$CURRENT_DATE.dump"

export PGPASSWORD="14341225"

pg_dump -h echolink_db -U $DB_USER -d $DB_NAME -F c -b -v -f $BACKUP_FILE

find $BACKUP_DIR -type f -mtime -7 -name "*.dump" -exec rm {} \;