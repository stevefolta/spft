{
	description = "Simple Proportional-Font Terminal";

	inputs = {
		};

	outputs = { self, nixpkgs }:
		let
			# Why doesn't the flakes build system handle this automatically?!
			forAllSystems = nixpkgs.lib.genAttrs nixpkgs.lib.systems.flakeExposed;
			nixpkgsFor = forAllSystems (system: import nixpkgs { inherit system; });
		in {
			packages = forAllSystems (system: {
				default =
					nixpkgsFor.${system}.stdenv.mkDerivation {
						name = "spft";
						src = self;
						buildInputs = with nixpkgsFor.${system}; [ xorg.libXft pkg-config ];
						installPhase = ''
							mkdir -p $out/bin $out/share/man/man1
							cp spft els end-tabs $out/bin/
							cp spft.1 $out/share/man/man1
							'';
						};
					});
			};
}


