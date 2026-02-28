#!/bin/bash
# Build and install Watt-32 TCP/IP library for DOSCODE
# Downloads source, builds OpenWatcom library
# Usage: build_tcplib.sh [--32bit] [--uninstall]
set -e

# --- Parse arguments ---
BUILD_32BIT=0
DO_UNINSTALL=0
for arg in "$@"; do
    case "$arg" in
        --32bit)     BUILD_32BIT=1 ;;
        --uninstall) DO_UNINSTALL=1 ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WATT32_INC="$SCRIPT_DIR/lib/watt32/inc"
WATT32_LIB="$SCRIPT_DIR/lib/watt32/lib"
WATCOM="${WATCOM:-/opt/watcom}"

# --- Build-mode variables ---
if [ $BUILD_32BIT -eq 1 ]; then
    W32_CC="wcc386"
    W32_CFLAGS="-mf -3s -bt=dos -ox -zq -fr=nul -wx -fpi -DWATT32_BUILD"
    W32_OBJDIR="flat"
    W32_TMP_LIB="wattcpwf.lib"
    W32_INST_LIB="wattcpwf.lib"
    W32_CFLAGS_SHORT="-mf -3s -bt=dos -ox -zq"
    W32_DESC="32-bit flat model"
    W32_AFLAGS="-bt=dos -3r -mf -dDOSX -dDOS4GW -zq -fr=nul -w3 -d1"
else
    W32_CC="wcc"
    W32_CFLAGS="-ml -0 -os -zc -s -bt=dos -zq -fr=nul -wx -fpi -DWATT32_BUILD"
    W32_OBJDIR="large"
    W32_TMP_LIB="wattcpwl.lib"
    W32_INST_LIB="wattcpl.lib"
    W32_CFLAGS_SHORT="-ml -0 -os -zc -s -bt=dos"
    W32_DESC="16-bit large model"
    W32_AFLAGS="-bt=dos -zq -fr=nul -w3 -d1"
fi

# Known Watt-32 header subdirectories (installed by this script)
WATT32_SUBDIRS="arpa net netinet netinet6 protocol rpc rpcsvc sys w32-fakes"

# --- Uninstall ---
if [ $DO_UNINSTALL -eq 1 ]; then
    REMOVED=0

    # Remove library files (both 16-bit and 32-bit)
    for lib in wattcpl.lib wattcpwf.lib; do
        if [ -f "$WATT32_LIB/$lib" ]; then
            rm -f "$WATT32_LIB/$lib"
            echo "Removed $WATT32_LIB/$lib"
            REMOVED=$((REMOVED+1))
        fi
    done

    # Remove known Watt-32 header subdirectories
    for d in $WATT32_SUBDIRS; do
        if [ -d "$WATT32_INC/$d" ]; then
            rm -rf "$WATT32_INC/$d"
            echo "Removed $WATT32_INC/$d/"
            REMOVED=$((REMOVED+1))
        fi
    done

    # Remove known top-level Watt-32 headers (only specific files, not a glob)
    for f in tcp.h netdb.h resolv.h syslog.h err.h poll.h copying.bsd; do
        if [ -f "$WATT32_INC/$f" ]; then
            rm -f "$WATT32_INC/$f"
            echo "Removed $WATT32_INC/$f"
            REMOVED=$((REMOVED+1))
        fi
    done

    # Warn about any remaining files we didn't touch
    REMAINING=$(find "$WATT32_INC" -mindepth 1 2>/dev/null | head -5)
    if [ -n "$REMAINING" ]; then
        echo ""
        echo "Note: these files were NOT removed (not installed by this script):"
        find "$WATT32_INC" -mindepth 1
    fi

    if [ $REMOVED -eq 0 ]; then
        echo "Nothing to uninstall."
    else
        echo "Watt-32 uninstalled."
    fi
    exit 0
fi

# --- Install ---

# Idempotency — skip if already installed
if [ -f "$WATT32_LIB/$W32_INST_LIB" ] && [ -f "$WATT32_INC/tcp.h" ]; then
    echo "Watt-32 ($W32_DESC) already installed"
    ls -la "$WATT32_LIB/$W32_INST_LIB"
    exit 0
fi

# Check OpenWatcom
if [ ! -x "$WATCOM/binl/$W32_CC" ]; then
    echo "ERROR: OpenWatcom $W32_CC not found at $WATCOM" >&2
    echo "Run: bash dosagent/install_compiler.sh" >&2
    exit 1
fi

export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"

echo "=== Installing Watt-32 TCP/IP library ($W32_DESC) ==="

# Download sezero/watt32 fork (has pre-generated syserr.c)
GITHUB_URL="https://github.com/sezero/watt32/archive/refs/heads/2.2.11-sezero.tar.gz"
TMP_DIR=$(mktemp -d /tmp/watt32_build_XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT

echo "Downloading Watt-32 source..."
curl -fsSL "$GITHUB_URL" | tar xz -C "$TMP_DIR" --strip-components=1

SRCDIR="$TMP_DIR/src"
OBJDIR="$SRCDIR/build/watcom/$W32_OBJDIR"
mkdir -p "$OBJDIR"

# --- Generate required build files ---

# cflags.h — version.c includes this
cat > "$OBJDIR/cflags.h" << CFLAGS_H
const char *w32_cflags = "$W32_CFLAGS_SHORT";
const char *w32_cc     = "$W32_CC";
CFLAGS_H

# cflags_buf.h — version.c includes this as a byte array
CFLAGS_STR="$W32_CFLAGS_SHORT -wx -fpi -DWATT32_BUILD"
python3 -c "s='$CFLAGS_STR'; print(','.join(str(ord(c)) for c in s)+',')" \
    > "$OBJDIR/cflags_buf.h"

# Patch syserr.c — add _WCDATA qualifier to match stdlib.h declaration
sed -i 's/^char \*SYS_ERRLIST\[\]/char * _WCDATA SYS_ERRLIST[]/' \
    "$SRCDIR/build/watcom/syserr.c"

# Patch config.h — enable features for the target memory model
if [ $BUILD_32BIT -eq 1 ]; then
    # 32-bit flat model: replace or add __FLAT__ feature block
    python3 << PYEOF
import re
with open('$SRCDIR/config.h', 'r') as f:
    c = f.read()
block = '''#if !defined(OPT_DEFINED) && (defined(__FLAT__) || defined(__386__))
  #define USE_BOOTP
  #define USE_DHCP
  #define USE_LANGUAGE
  #define USE_FRAGMENTS
  #define USE_LOOPBACK
  #define USE_BUFFERED_IO
  #define OPT_DEFINED
#endif'''
# Try replacing existing __FLAT__ block
m = re.search(r'#if !defined\(OPT_DEFINED\)\s*&&\s*defined\(__FLAT__\).*?#endif', c, re.DOTALL)
if m:
    c = c[:m.start()] + block + c[m.end():]
else:
    # No __FLAT__ block found; append
    c += '\n' + block + '\n'
with open('$SRCDIR/config.h', 'w') as f:
    f.write(c)
PYEOF
else
    # 16-bit large model: existing patch
    python3 -c "
with open('$SRCDIR/config.h', 'r') as f:
    c = f.read()
c = c.replace(
    '''#if !defined(OPT_DEFINED) && defined(__LARGE__)
  #define USE_DEBUG
  #define OPT_DEFINED
#endif''',
    '''#if !defined(OPT_DEFINED) && defined(__LARGE__)
  #define USE_BOOTP
  #define USE_DHCP
  #define USE_LANGUAGE
  #define USE_FRAGMENTS
  #define USE_LOOPBACK
  #define USE_BUFFERED_IO
  #define OPT_DEFINED
#endif''')
with open('$SRCDIR/config.h', 'w') as f:
    f.write(c)
"
fi

# --- Compile all library source files ---

FULL_CFLAGS="$W32_CFLAGS -I$SRCDIR -I$TMP_DIR/inc -I$WATCOM/h"

CORE_SOURCE="bsdname.c btree.c chksum.c country.c crc.c dynip.c
echo.c fortify.c getopt.c gettod.c highc.c idna.c
ip4_frag.c ip4_in.c ip4_out.c ip6_in.c ip6_out.c language.c
lookup.c loopback.c misc.c netback.c oldstuff.c packet32.c
pc_cbrk.c pcarp.c pcbootp.c pcbuf.c pcconfig.c pcdbug.c
pcdhcp.c pcdns.c pcicmp.c pcicmp6.c pcigmp.c pcintr.c
pcping.c pcpkt.c pcpkt32.c pcqueue.c pcrarp.c pcrecv.c
pcsed.c pcslip.c pcstat.c pctcp.c ports.c powerpak.c ppp.c
pppoe.c profile.c punycode.c qmsg.c rs232.c run.c
settod.c sock_dbu.c sock_in.c sock_ini.c sock_io.c sock_prn.c
sock_scn.c sock_sel.c split.c strings.c swsvpkt.c tcp_fsm.c
tcp_md5.c tftp.c timer.c udp_rev.c version.c wdpmi.c
win_dll.c winadinf.c winmisc.c winpkt.c x32vm.c"

BSD_SOURCE="accept.c bind.c bsddbug.c close.c connect.c fcntl.c
fsext.c get_ai.c get_ip.c get_ni.c get_xbyr.c geteth.c
gethost.c gethost6.c getname.c getnet.c getprot.c getput.c
getserv.c ioctl.c linkaddr.c listen.c netaddr.c neterr.c
nettime.c nsapaddr.c poll.c presaddr.c printk.c receive.c
select.c shutdown.c signal.c socket.c sockopt.c stream.c
syslog.c syslog2.c transmit.c"

BIND_SOURCE="res_comp.c res_data.c res_debu.c res_init.c res_loc.c
res_mkqu.c res_quer.c res_send.c"

ZLIB_SOURCE="zadler32.c zcompres.c zcrc32.c zgzio.c zuncompr.c
zdeflate.c ztrees.c zutil.c zinflate.c zinfback.c zinftree.c
zinffast.c"

echo "Compiling C sources..."
FAIL=0
COUNT=0
cd "$SRCDIR"
for f in $CORE_SOURCE $BSD_SOURCE $BIND_SOURCE $ZLIB_SOURCE; do
    $W32_CC $FULL_CFLAGS -fo="$OBJDIR/${f%.c}.obj" "$f" 2>/dev/null
    rc=$?
    COUNT=$((COUNT+1))
    if [ $rc -ne 0 ]; then
        echo "  FAIL: $f" >&2
        FAIL=$((FAIL+1))
    fi
done
echo "  Compiled $COUNT C files ($FAIL failures)"

if [ $FAIL -ne 0 ]; then
    echo "ERROR: Some files failed to compile" >&2
    exit 1
fi

# Assemble .asm files
echo "Assembling..."
for f in asmpkt.asm chksum0.asm cpumodel.asm; do
    wasm $W32_AFLAGS -fo="$OBJDIR/${f%.asm}.obj" "$f"
done

# Create library
echo "Creating library..."
LIBFILE="$TMP_DIR/lib/$W32_TMP_LIB"
mkdir -p "$TMP_DIR/lib"
rm -f "$LIBFILE"
ARGS=""
for obj in "$OBJDIR"/*.obj; do
    ARGS="$ARGS +$obj"
done
wlib -q -b -c "$LIBFILE" $ARGS

# --- Install into project tree ---

echo "Installing headers..."
mkdir -p "$WATT32_INC" "$WATT32_LIB"
cp -r "$TMP_DIR/inc/"* "$WATT32_INC/"

echo "Installing library..."
cp "$LIBFILE" "$WATT32_LIB/$W32_INST_LIB"

# --- Verify ---

if [ ! -f "$WATT32_INC/tcp.h" ]; then
    echo "ERROR: tcp.h not found after install" >&2
    exit 1
fi

if [ ! -f "$WATT32_LIB/$W32_INST_LIB" ]; then
    echo "ERROR: $W32_INST_LIB not found after install" >&2
    exit 1
fi

echo "=== Watt-32 installed successfully ==="
echo "  Headers: $WATT32_INC/ ($(ls "$WATT32_INC/" | wc -l) top-level entries)"
echo "  Library: $WATT32_LIB/$W32_INST_LIB ($(stat -c%s "$WATT32_LIB/$W32_INST_LIB") bytes)"
