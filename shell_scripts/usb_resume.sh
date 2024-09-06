#!/system/bin/sh

USB_CMD= 	(0xfe200020)
USB_STATUS=	(0xfe200024)
IMAN_0=		(0xfe200460)
PORTSC=		(0xfe200420)

ERST_START=	(0x78078000)

#Usage: read $REG_name
read() 
{
	echo -n "Read $1: "
	./devmem $1
}

write() 
{
	echo -n "Write $2 to $1\n"
	./devmem $1 w $2
}

echo "Current state..."
read $USB_CMD
read $USB_STATUS
read $IMAN_0
read $PORTSC

echo "\nResume USB..."
write $USB_CMD 0x5
read $USB_CMD
read $USB_STATUS
read $IMAN_0
read $PORTSC

