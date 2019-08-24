#! /bin/sh

# Migrate config if necessary

if [ ! -f $SNAP_COMMON/config/framework.config ]; then
    mkdir -p $SNAP_COMMON/config
    cp $SNAP/etc/cups/framework.config $SNAP_COMMON/config/framework.config
fi

exec $SNAP/bin/server