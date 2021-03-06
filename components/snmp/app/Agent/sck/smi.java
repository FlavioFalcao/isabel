// -----------------------smi
// Copyright (C) 1998  Yves Soun.

// This program is free software; you can redistribute it and/or
// modify it under the terms of the Affero GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// Affero GNU General Public License for more details (cf. file COPYING).

package sck;
import java.io.*;

/** Smi Objects are immutable : after been created, they can't be modified.
 *  <BR>The avantage is to be able to use multiple pointers on the object without  
 *  take care of object cloning (refer to adoption, const pointers in C++).
 *  <P>These kind of objects are thread-safe.
 *
 * @author Yves Soun.
 */

 //  Smi objects implements specific constructors:
 // - constructor(ByteArrayInputStream inBer) .
 // - constructor(String s) 
 //  equals() is also overloaded.

public abstract class smi{ 

  // Class
  static final byte UNIVERSAL         = (byte) 0x00;
  static final byte APPLICATION       = (byte) 0x40;
  static final byte CONTEXTSPECIFIC   = (byte) 0x80;
  static final byte PRIVATE           = (byte) 0xC0;
 
  static final byte PRIMITIVE         = (byte) 0x00;
  static final byte CONSTRUCT         = (byte) 0x20;
 
  // SNMP specific construction.
  static final byte INTEGER_tag          =(byte) ( UNIVERSAL   | PRIMITIVE | 0x02 );
  static final byte OCTETSTRING_tag      =(byte) ( UNIVERSAL   | PRIMITIVE | 0x04 );
  static final byte NULL_tag             =(byte) ( UNIVERSAL   | PRIMITIVE | 0x05 );
  static final byte OID_tag              =(byte) ( UNIVERSAL   | PRIMITIVE | 0x06 );
  static final byte SEQUENCE_tag         =(byte) ( UNIVERSAL   | CONSTRUCT | 0x10 ); 
 
  static final byte IPADDRESS_tag        =(byte) ( APPLICATION | PRIMITIVE | 0x00 );
  static final byte COUNTER_tag          =(byte) ( APPLICATION | PRIMITIVE | 0x01 );
  static final byte GAUGE_tag            =(byte) ( APPLICATION | PRIMITIVE | 0x02 );
  static final byte TIMETICKS_tag        =(byte) ( APPLICATION | PRIMITIVE | 0x03 );
  static final byte OPAQUE_tag           =(byte) ( APPLICATION | PRIMITIVE | 0x04 );
 
  protected byte _tag ; 

  /** Used only by subclasses.
   *  No default constructor : tag must be known for object creation.
   */
  protected smi(byte tag){
    this._tag = tag;
  }

  /** Compares two Smi Objects for equality.
   *  <BR>This method is overloaded by subclasses.
   *  @return true if this Smi Object is the same as the obj argument, false otherwise.
   */
  public boolean equals (Object obj) {
    if (obj == null)
      return false;
    if (getClass() != obj.getClass()) // tester _tag n'est pas suffisant pour les types SEQUENCE
      return false;
    return true;
  }

  /** Returns the value of this Integer as a String.
   *  Complex objects (subclasses of construct) don't use this method: println() is used.
   *  @see sck.sequence 
   *  @see sck.smi#println
   */  
  public abstract String toString();

  /** Returns a Message in a StringBuffer.
   * @param tab indentation.
   */
  public void println(String tab, StringBuffer sb){
    sb.append(tab).append(toString()).append(" (").append(getShortClassName()).append(")\n");
  }

  /** Returns short class name (not sck.className) .
   *   Should be in util.xx.getShortClassName(this)
   */
  public String getShortClassName(){
    String FullTypeSmi = getClass().getName(); // renvoie full class name (sck.xx)
    return (FullTypeSmi.substring(FullTypeSmi.lastIndexOf('.')+1)); // renvoie xx
  }

  /** Returns Ber coding (tag + length + value).
   *  Null overloads this method.
   *  @see Null
   *  @return byte[] Ber coding.
   */
  public byte[] codeBer(){
    byte[] _berValeur, _berLongueur; // contient codage BER de la longueur et valeur de l'objet smi
    byte[] _code; //

    _berValeur= codeValeur();
    _berLongueur = this.codeLongueur(_berValeur.length);
    _code = new byte [_berLongueur.length + _berValeur.length +1];
    _code[0] = this._tag;
    System.arraycopy(_berLongueur,0,_code,1,_berLongueur.length);
    System.arraycopy(_berValeur,0,_code,_berLongueur.length +1,_berValeur.length);

    return _code;     
  }

  /** Returns Ber coding of the value.
   *  @see #CodeBer
   */
  abstract byte[] codeValeur();

  /** Returns Ber coding of the length of the value Ber coding.
   *  @param lgBerValeur: Ber Coding length of the value .
   */
  private final byte[] codeLongueur(int lgBerValeur) { 
    byte[] lg;
    if (lgBerValeur < 0x80){
      lg = new byte[1];
      lg[0] = (byte) lgBerValeur;
    } else if (lgBerValeur <= 0xFF){
      lg = new byte[2];
      lg[0] = (byte) 0x81;
      lg[1] = (byte) lgBerValeur;
    } else {   // lgBerValeur <= 0xFFFF (taille max UDP)
      lg = new byte[3];
      lg[0] = (byte) 0x82;
      lg[1] = (byte)(lgBerValeur >> 8);
      lg[2] = (byte)(lgBerValeur);
    }
    return (lg);
  }

 /** The value of this object is get from a ByteArrayInputStream holding Ber Coding.
  *  Can be only used by constructors (cause immutable object).
  *  The Smi object tag must be removed from the stream before calling this method.
  *  @return number of bytes read from the stream
  */
  protected final int decodeBer(ByteArrayInputStream inBer) throws IOException{
    int[] lg_BerValeur = decodeLongueur(inBer);
    decodeValeur(inBer, lg_BerValeur[1]);
    return ( lg_BerValeur[0] + lg_BerValeur[1]);
  }

  /** Returns an array:
   *  int[0] = length of length Ber Coding.
   *  int[1] = length of value Ber coding.
   *  @param buffer holds Ber coding for this Object
   *  
   */
  private final int[] decodeLongueur(ByteArrayInputStream bufferBer){
  int nbBytes, lgBerValeur, n;
  byte[] BerLongueur;

  int b = bufferBer.read() ;
  nbBytes = b & 0x80 ;   
  if ( nbBytes == 0){
    lgBerValeur = b;
    nbBytes = 0;
  }else{
    nbBytes = b & 0x7f; // numbet of bytes coding length
    BerLongueur = new byte[nbBytes];
    bufferBer.read(BerLongueur,0,nbBytes);

    lgBerValeur = 0;
    for (n=0; n < nbBytes; n++)
      lgBerValeur = (lgBerValeur << 8) | ( ((int)BerLongueur[n]) & 0xff);
  }

  return new int[]{++nbBytes, lgBerValeur};
  }

  /** Used by decodeber()
   *  Specific to each Smi subclass.
   */
  abstract void decodeValeur(ByteArrayInputStream bufferBer, int lgBerValeur) throws IOException;
}
