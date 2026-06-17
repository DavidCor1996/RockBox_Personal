#!/usr/bin/env bash
set -euo pipefail

SHARE_PATH="${1:-/home/david/Music}"
SHARE_USER="${SUDO_USER:-${USER}}"
HOST_NAME="$(hostname)"

if [[ ! -d "$SHARE_PATH" ]]; then
  echo "Music folder not found: $SHARE_PATH" >&2
  exit 1
fi

echo "Installing Samba if needed..."
sudo pacman -S --needed samba

echo "Writing Samba configuration..."
sudo install -d -m 0755 /etc/samba
if [[ -f /etc/samba/smb.conf ]]; then
  sudo cp -a /etc/samba/smb.conf "/etc/samba/smb.conf.backup.$(date +%Y%m%d%H%M%S)"
fi

tmp_conf="$(mktemp)"
cat > "$tmp_conf" <<EOF
[global]
   workgroup = WORKGROUP
   server string = ${HOST_NAME}
   server role = standalone server
   security = user
   passdb backend = tdbsam
   map to guest = Bad User

   load printers = no
   printing = bsd
   disable spoolss = yes

   vfs objects = catia fruit streams_xattr
   fruit:aapl = yes
   fruit:metadata = stream
   fruit:model = MacSamba
   ea support = yes

[Music]
   path = ${SHARE_PATH}
   valid users = ${SHARE_USER}
   browseable = yes
   read only = no
   writable = yes
   create mask = 0664
   directory mask = 0775
   force user = ${SHARE_USER}
   force group = ${SHARE_USER}
EOF
sudo install -m 0644 "$tmp_conf" /etc/samba/smb.conf
rm -f "$tmp_conf"

echo "Validating Samba configuration..."
testparm -s /etc/samba/smb.conf >/dev/null

echo "Setting ownership/permissions on ${SHARE_PATH}..."
sudo chown -R "${SHARE_USER}:${SHARE_USER}" "$SHARE_PATH"
chmod u+rwX,g+rwX "$SHARE_PATH"

echo
echo "Now set the Samba password for ${SHARE_USER}."
echo "This is the password your Mac will use for smb://${HOST_NAME}.local/Music or smb://$(ip -4 addr show wlan0 | awk '/inet / {print $2}' | cut -d/ -f1)/Music"
sudo smbpasswd -a "${SHARE_USER}"
sudo smbpasswd -e "${SHARE_USER}" >/dev/null

if command -v ufw >/dev/null 2>&1 && sudo ufw status | grep -q "Status: active"; then
  echo "Opening Samba in UFW..."
  sudo ufw allow Samba || {
    sudo ufw allow 445/tcp
    sudo ufw allow 139/tcp
    sudo ufw allow 137/udp
    sudo ufw allow 138/udp
  }
fi

echo "Advertising SMB to macOS through Avahi..."
sudo install -d -m 0755 /etc/avahi/services
tmp_avahi="$(mktemp)"
cat > "$tmp_avahi" <<EOF
<?xml version="1.0" standalone='no'?>
<!DOCTYPE service-group SYSTEM "avahi-service.dtd">
<service-group>
  <name replace-wildcards="yes">%h Music</name>
  <service>
    <type>_smb._tcp</type>
    <port>445</port>
  </service>
</service-group>
EOF
sudo install -m 0644 "$tmp_avahi" /etc/avahi/services/smb.service
rm -f "$tmp_avahi"

echo "Enabling services..."
sudo systemctl enable --now smb.service
sudo systemctl enable --now nmb.service || true
sudo systemctl enable --now avahi-daemon.service
sudo systemctl restart smb.service avahi-daemon.service

echo
echo "Done."
echo "From your Mac: Finder > Go > Connect to Server:"
echo "  smb://${HOST_NAME}.local/Music"
echo "or:"
echo "  smb://$(ip -4 addr show wlan0 | awk '/inet / {print $2}' | cut -d/ -f1)/Music"
