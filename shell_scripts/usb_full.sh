#!/system/bin/sh

./usb_halt.sh
./usb_reconfig1.sh
./usb_resume.sh
./usb_halt.sh
./usb_reconfig2.sh
./usb_resume.sh
