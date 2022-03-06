import Lake
open Lake DSL

package foo {
  libRoots := #[]
  dependencies := #[{
    name := `OpenSSL
    src := Source.path "../.."
    args := ["static"]
  }]
}
