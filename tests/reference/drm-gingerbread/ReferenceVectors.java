package org.apache.harmony.security.provider.crypto;

import java.nio.charset.StandardCharsets;
import java.util.Base64;
import java.util.Random;
import javax.crypto.Cipher;
import javax.crypto.spec.SecretKeySpec;

/** Executes the unmodified AOSP Gingerbread SHA1PRNG implementation. */
public final class ReferenceVectors {
    private static final long SEED = 123456789L;

    private static String encrypt(byte[] key, String plain) throws Exception {
        Cipher cipher = Cipher.getInstance("AES/ECB/PKCS5Padding");
        cipher.init(Cipher.ENCRYPT_MODE, new SecretKeySpec(key, "AES"));
        return Base64.getEncoder().encodeToString(
            cipher.doFinal(plain.getBytes(StandardCharsets.UTF_8)));
    }

    public static void main(String[] arguments) throws Exception {
        SHA1PRNG_SecureRandomImpl generator = new SHA1PRNG_SecureRandomImpl();
        generator.engineSetSeed("SFHPnull".getBytes(StandardCharsets.UTF_8));
        byte[] key = new byte[16];
        generator.engineNextBytes(key);
        StringBuilder hex = new StringBuilder();
        for (byte b : key) hex.append(String.format("%02x", b & 255));
        System.out.println("KEY\t" + hex);
        System.out.println("SEED\t" + SEED);
        Random random = new Random(SEED);
        for (char suffix = 'a'; suffix <= 't'; suffix++) {
            String plain;
            if (suffix == 'd') {
                plain = random.nextLong() + "#null#" + random.nextInt() + "#1188";
            } else if (suffix == 'l') {
                plain = random.nextLong() + "#null#0";
            } else if (suffix == 's') {
                plain = "http://confirmation.gameloft.com/partners/android/validate_key.php?key=#KEY#&product=#PRODUCT_ID#&imei=#ID#";
            } else {
                plain = random.nextLong() + "#0#" + random.nextInt() + "#" + random.nextInt();
            }
            System.out.println("gl_" + suffix + "\t" + plain + "\t" + encrypt(key, plain));
        }
        for (String plain : new String[] {"", "1234567890123456", "bad#count", "n#other#2"}) {
            System.out.println("VECTOR\t" + plain + "\t" + encrypt(key, plain));
        }
    }
}
