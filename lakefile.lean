import Lake
open System Lake DSL

def leanSoureDir := "."
def cppCompiler := "clang"
def cDir : FilePath := "bindings"
def ffiSrc := cDir / "openssl-shim.c"
def ffiO := "openssl-shim.o"
def ffiLib := "libopenssl-shim.a"

def ffiOTarget (pkgDir : FilePath) (sslIncludeDir : FilePath) : FileTarget :=
  let oFile := pkgDir / defaultBuildDir / cDir / ffiO
  let srcTarget := inputFileTarget <| pkgDir / ffiSrc
  fileTargetWithDep oFile srcTarget fun srcFile => do
    compileO oFile srcFile
      #["-I", (← getLeanIncludeDir).toString, "-I", sslIncludeDir.toString] cppCompiler

def cLibTarget (pkgDir : FilePath) (sslIncludeDir : FilePath) : FileTarget :=
  let libFile := pkgDir / defaultBuildDir / cDir / ffiLib
  staticLibTarget libFile #[ffiOTarget pkgDir sslIncludeDir]

def openSSLTargets (dir : FilePath) (static : Bool) : Array FileTarget := 
  let ext := if static then "a" else sharedLibExt
  #["libssl", "libcrypto"]
    |>.map (dir / "lib" / s!"{·}.{ext}")
    |>.map inputFileTarget

package OpenSSL (pkgDir) (args) do
  -- get OPENSSL_DIR information from environment variables
  let openSSLDirEnv ← IO.getEnv "OPENSSL_DIR"
  let useStatic := args.contains "static"
  -- fail if OPENSSL_DIR not provided
  if let some openSSLDir := FilePath.mk <$> openSSLDirEnv then
    let sslIncludeDir := openSSLDir / "include"
    return {
      name := `OpenSSL
      defaultFacet := PackageFacet.staticLib
      srcDir := "src"
      moreLibTargets :=  #[cLibTarget pkgDir sslIncludeDir] ++ openSSLTargets openSSLDir useStatic
    }
  else
    throw <| IO.userError "Please set OPENSSL_DIR to the path of the openssl directory"
