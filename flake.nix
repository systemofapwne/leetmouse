{
  description = "LEETMOUSE - Quake-live like mouse acceleration for Linux";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
  }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = nixpkgs.legacyPackages.${system};

      leetmouse-driver = pkgs.stdenv.mkDerivation rec {
        pname = "leetmouse-driver";
        version = "0.9.0";

        src = ./.;

        nativeBuildInputs = with pkgs; [
          gnumake
          kmod
        ];

        buildInputs = with pkgs; [
          linuxPackages.kernel.dev
        ];

        hardeningDisable = ["pic" "format"];

        makeFlags = [
          "KERNELDIR=${pkgs.linuxPackages.kernel.dev}/lib/modules/${pkgs.linuxPackages.kernel.modDirVersion}/build"
        ];

        preBuild = ''
          # Copy sample config if config.h doesn't exist
          if [ ! -f driver/config.h ]; then
            cp driver/config.sample.h driver/config.h
          fi
        '';

        buildPhase = ''
          runHook preBuild
          make driver
          runHook postBuild
        '';

        installPhase = ''
          runHook preInstall
          mkdir -p $out/lib/modules/${pkgs.linuxPackages.kernel.modDirVersion}/kernel/drivers/usb
          cp driver/leetmouse.ko $out/lib/modules/${pkgs.linuxPackages.kernel.modDirVersion}/kernel/drivers/usb/

          # Install udev rules and scripts
          mkdir -p $out/lib/udev/rules.d
          mkdir -p $out/lib/udev
          cp install_files/udev/99-leetmouse.rules $out/lib/udev/rules.d/
          cp install_files/udev/leetmouse_bind $out/lib/udev/
          cp install_files/udev/leetmouse_manage $out/lib/udev/
          chmod +x $out/lib/udev/leetmouse_bind
          chmod +x $out/lib/udev/leetmouse_manage
          runHook postInstall
        '';

        meta = with pkgs.lib; {
          description = "Quake-live like mouse acceleration for Linux";
          homepage = "https://github.com/systemofapwne/leetmouse";
          license = licenses.gpl3;
          platforms = platforms.linux;
          maintainers = [];
        };
      };
    in {
      packages = {
        default = leetmouse-driver;
        leetmouse-driver = leetmouse-driver;
      };

      devShells.default = pkgs.mkShell {
        buildInputs = with pkgs; [
          gnumake
          gcc
          kmod
          linuxPackages.kernel.dev
          udev
        ];

        shellHook = ''
          echo "LEETMOUSE development environment"
          echo "Available commands:"
          echo "  make driver       - Build the kernel module"
          echo "  make clean        - Clean build artifacts"
          echo "  make driver_clean - Clean driver build artifacts"
          echo ""
          echo "Kernel headers: ${pkgs.linuxPackages.kernel.dev}/lib/modules/${pkgs.linuxPackages.kernel.modDirVersion}/build"
        '';
      };

      # NixOS module for easy installation
      nixosModules.default = {
        config,
        lib,
        pkgs,
        ...
      }:
        with lib; let
          cfg = config.services.leetmouse;
        in {
          options.services.leetmouse = {
            enable = mkEnableOption "LEETMOUSE driver";

            package = mkOption {
              type = types.package;
              default = self.packages.${pkgs.system}.leetmouse-driver;
              description = "The LEETMOUSE package to use";
            };
          };

          config = mkIf cfg.enable {
            boot.extraModulePackages = [cfg.package];
            boot.kernelModules = ["leetmouse"];

            services.udev.packages = [cfg.package];

            # Ensure the module loads on boot
            systemd.services.leetmouse-load = {
              description = "Load LEETMOUSE kernel module";
              wantedBy = ["multi-user.target"];
              after = ["systemd-udev-settle.service"];
              serviceConfig = {
                Type = "oneshot";
                RemainAfterExit = true;
                ExecStart = "${pkgs.kmod}/bin/modprobe leetmouse";
                ExecStop = "${pkgs.kmod}/bin/modprobe -r leetmouse";
              };
            };
          };
        };
    });
}
