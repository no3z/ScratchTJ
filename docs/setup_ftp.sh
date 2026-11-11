#!/bin/bash
# FTP access to the ScratchTJ beats and samples folders
set -e
export DEBIAN_FRONTEND=noninteractive
which vsftpd >/dev/null 2>&1 || sudo apt-get install -y -q vsftpd 2>&1 | tail -1

# The FTP root holds only the two music folders, bind-mounted from the
# places xwax reads them. The root itself belongs to root: vsftpd refuses
# a chroot the user could write to.
sudo mkdir -p /srv/scratchtj/beats /srv/scratchtj/samples
sudo chown root:root /srv/scratchtj && sudo chmod 755 /srv/scratchtj
[ -f /etc/fstab.pre-scratchtj-ftp ] || sudo cp -p /etc/fstab /etc/fstab.pre-scratchtj-ftp
grep -q "/srv/scratchtj/beats" /etc/fstab   || echo "/home/no3z/beats    /srv/scratchtj/beats    none  bind,nofail  0  0" | sudo tee -a /etc/fstab >/dev/null
grep -q "/srv/scratchtj/samples" /etc/fstab || echo "/home/no3z/samples  /srv/scratchtj/samples  none  bind,nofail  0  0" | sudo tee -a /etc/fstab >/dev/null
sudo systemctl daemon-reload 2>/dev/null || true
mountpoint -q /srv/scratchtj/beats   || sudo mount /srv/scratchtj/beats
mountpoint -q /srv/scratchtj/samples || sudo mount /srv/scratchtj/samples

[ -f /etc/vsftpd.conf.pre-scratchtj ] || sudo cp -p /etc/vsftpd.conf /etc/vsftpd.conf.pre-scratchtj
sudo tee /etc/vsftpd.conf >/dev/null <<'EOF'
# ScratchTJ: FTP access to the beats and samples folders
listen=YES
listen_ipv6=NO

# Log in as the Pi user; no anonymous access
anonymous_enable=NO
local_enable=YES
write_enable=YES
local_umask=022

# Only no3z may log in, and sees nothing but the two music folders
userlist_enable=YES
userlist_deny=NO
userlist_file=/etc/vsftpd.userlist
chroot_local_user=YES
local_root=/srv/scratchtj

# Passive mode on a fixed, small port range
pasv_enable=YES
pasv_min_port=40000
pasv_max_port=40050

utf8_filesystem=YES
use_localtime=YES
xferlog_enable=YES
connect_from_port_20=YES
secure_chroot_dir=/var/run/vsftpd/empty
pam_service_name=vsftpd
seccomp_sandbox=NO
EOF
echo no3z | sudo tee /etc/vsftpd.userlist >/dev/null
sudo systemctl enable vsftpd >/dev/null 2>&1
sudo systemctl restart vsftpd
sleep 1
systemctl is-active vsftpd
mount | grep scratchtj
sudo passwd -S no3z | awk '{print "password status:", $2}'
tail -3 /etc/fstab
echo DONE
