import Lake
open System Lake DSL

def leanSoureDir := "."
def cppCompiler := "clang"
def cDir : FilePath := "bindings"
def ffiSrc := cDir / "openssl-shim.c"
def ffiO := "openssl-shim.o"
def ffiLib := "libopenssl-shim.a"

def ffiOTarget (pkgDir : FilePath) : FileTarget :=
  let oFile := pkgDir / defaultBuildDir / cDir / ffiO
  let srcTarget := inputFileTarget <| pkgDir / ffiSrc
  fileTargetWithDep oFile srcTarget fun srcFile => do
    compileO oFile srcFile
      #["-I", (← getLeanIncludeDir).toString] cppCompiler

def cLibTarget (pkgDir : FilePath) : FileTarget :=
  let libFile := pkgDir / defaultBuildDir / cDir / ffiLib
  staticLibTarget libFile #[ffiOTarget pkgDir]

package OpenSSL (pkgDir) (args) {
  defaultFacet := PackageFacet.staticLib
  srcDir := "src"
  moreLibTargets :=  #[cLibTarget pkgDir]
}
