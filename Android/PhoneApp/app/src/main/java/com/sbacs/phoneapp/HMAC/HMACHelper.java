package com.sbacs.phoneapp.HMAC;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.security.NoSuchAlgorithmException;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.KeySpec;
import java.util.HashMap;
import java.util.Map;

import javax.crypto.Mac;
import javax.crypto.SecretKey;
import javax.crypto.SecretKeyFactory;
import javax.crypto.spec.PBEKeySpec;
import javax.crypto.spec.SecretKeySpec;

/**
 * Created by Richard on 2017-02-05.
 */

public class HMACHelper {
    private static final int iterations = 10000;
    private static final int secret_length = 255 * 8; // Secret length is in bits

    public static Map<String, String> GetHMACHeaders(int userId, String body, String key) {
        String secret;
        Map <String, String> result = new HashMap<String, String>();

        try {
            secret = URLEncoder.encode(SecretString(body, key), "UTF-16");
        } catch (UnsupportedEncodingException e) {
            secret = "";
        }

        result.put("hmac-user", "" + userId);
        result.put("hmac-content", secret);
		
        return result;
    }

    public static byte[] HMACLoginBody(String username, String password) {
        JSONObject body = new JSONObject();
        try {
            body.put("username", username);
            body.put("password", password);
        } catch (JSONException e) {
        }

        return body.toString().getBytes();
    }

    private static String SecretString(String body, String key){
        SecretKeyFactory skf;
        SecretKeySpec spec;
        byte[] secret;
        Mac mac;
        try {
			mac = Mac.getInstance("HmacSHA256");
        } catch (NoSuchAlgorithmException e) {
            return "";
        }

        spec = new SecretKeySpec(key.getBytes(), "HmacSHA256");

        try {
            mac.init(spec);
			secret = mac.doFinal(body.getBytes());
        } catch (Exception e) {
            return "";
        }

        return new String(secret);
    }
}
