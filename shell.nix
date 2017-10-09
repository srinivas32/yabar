with import <nixpkgs>{}; stdenv.mkDerivation {
  name = "yabar-dev";
  src = ./.;

  buildInputs = [
    cairo gdk_pixbuf libconfig pango pkgconfig xorg.xcbutilwm docbook_xsl
    alsaLib wirelesstools asciidoc libxslt makeWrapper libxml2 playerctl
  ];
}
