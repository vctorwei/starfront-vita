The four SHA1PRNG support files are unmodified AOSP Gingerbread Java sources,
retaining their Apache 2.0 notices. Source revision: `refs/heads/gingerbread` of
`platform/libcore-snapshot`; SHA1PRNG blob `c07e2b1203f0c9bdc67d5723261d6be95a4bc5ec`.

Source directory:
https://android.googlesource.com/platform/libcore-snapshot/+/refs/heads/gingerbread/luni/src/main/java/org/apache/harmony/security/provider/crypto/

SHA-256 of the downloaded source files:

- SHA1PRNG_SecureRandomImpl.java: bb81c8d6a922d144b66855a860e4666d753257f418205731bca33e7c05c51d0d
- SHA1Impl.java: eee1f6d92af2cb579a041fe385884e3abd05d50a526377654464e07091f1f67b
- SHA1_Data.java: fdc936446e53e2c1b3ccd45895ddbb674767dc7eba287cb9bef3bf3be119252a
- RandomBitsSupplier.java: f1e37a2c3401c1f4629b6ab6727c10e5b40ef7fdb38fdaf01fbfe71823a1f5fe

`ReferenceVectors.java` calls the original protected `engineSetSeed` and
`engineNextBytes` methods. The source's entropy provider is never used because
the seed is supplied before generating bytes. AES is requested explicitly as
ECB/PKCS5Padding (PKCS7 for AES's 16-byte blocks), and Base64 has no line breaks.
The wrapper also runs Java Random with seed 123456789 and the exact observed
GloftDRM.c() field order. These vectors contain no user serial or authorization.

To regenerate from the repository root with an available JDK:

```sh
javac -d work/tests/drm-java tests/reference/drm-gingerbread/*.java
java -cp work/tests/drm-java org.apache.harmony.security.provider.crypto.ReferenceVectors > tests/fixtures/drm-gingerbread.tsv
```
