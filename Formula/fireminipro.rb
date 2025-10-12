class Fireminipro < Formula
  desc "GUI frontend for the minipro programmer"
  homepage "https://github.com/Jartza/fireminipro"
  url "https://github.com/Jartza/fireminipro/archive/refs/tags/v0.0.2.tar.gz"
  sha256 "…"
  license "MIT"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "qt"    # or "qt@6" on Intel-only Macs
  depends_on "minipro"

  def install
    system "cmake", "-S", ".", "-B", "build",
                    "-GNinja",
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DCMAKE_PREFIX_PATH=#{Formula["qt"].opt_prefix}",
                    "-DCMAKE_INSTALL_RPATH=#{lib}"
    system "ninja", "-C", "build"
    bin.install "build/fireminipro"
    (share/"applications").install "path/to/fireminipro.desktop" if File.exist?("...")
  end
end
