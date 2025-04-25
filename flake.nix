{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };
  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        devShell = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [
            libpcap
            # speexdsp
            cmake
            ninja
            pkg-config
            glib
            libgpg-error
            libgcrypt
            pcre2
            c-ares
            brotli
            clang
            gnumake
          ];
          shellHook = ''
            ${pkgs.cmake}/bin/cmake -G Ninja \
            -DBUILD_wireshark=OFF \
            -DBUILD_tshark=ON \
            -DBUILD_rawshark=OFF \
            -DBUILD_editcap=OFF \
            -DBUILD_capinfos=OFF \
            -DBUILD_captype=OFF \
            -DBUILD_mergecap=OFF \
            -DBUILD_reordercap=OFF \
            -DBUILD_text2pcap=OFF \
            -DBUILD_dftest=OFF \
            -DBUILD_dcerpcidl2wrs=OFF \
            -DBUILD_androiddump=OFF \
            -DBUILD_sshdump=OFF \
            -DBUILD_ciscodump=OFF \
            -DBUILD_dpauxmon=OFF \
            -DBUILD_randpktdump=OFF \
            -DBUILD_randpkt=OFF \
            -DBUILD_dumpcap=ON \
            -DBUILD_rawshark=OFF \
            -DENABLE_ZLIB=OFF \
            -DENABLE_BROTLI=OFF \
            -DENABLE_PLUGINS=OFF \
            -DENABLE_QT6=OFF \
            -DENABLE_QT5=OFF \
            -DENABLE_GNUTLS=OFF \
            -DENABLE_LUA=OFF \
            -DENABLE_PORTAUDIO=OFF \
            -DENABLE_SPANDSP=OFF \
            -DENABLE_SINSP=OFF \
            -DENABLE_OPUS=OFF \
            -DENABLE_LIBXML2=OFF \
            -DENABLE_ILBC=OFF \
            -DENABLE_AMRNB=OFF \
            -DENABLE_BCG729=OFF \
            -DENABLE_KERBEROS= OFF \
            .

            ${pkgs.ninja}/bin/ninja
          '';
        };

      }
    );
}
