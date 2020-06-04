/*
  LFSC 1.5 reference grammar

  Authors: Cesare Tinelli
*/

grammar lfsc1_5;		

command
  : '(' 'declare' iden kind ')'
  | '(' 'declare' iden type ')'
  | '(' 'define' iden type ')'
  | '(' 'define' iden term ')'
  | '(' 'opaque' iden type ')'
  | '(' 'opaque' iden term ')'
  | '(' 'check' term ')'
  | '(' 'run' code ')'
  | '(' 'program' iden '(' typed_par+ ')' type code ')'
// Extension /////////////////////////////////
  | '(' 'declare-type' iden '(' type* ')' ')'
  | '(' 'declare-rule' iden ( ntype | vtype )+ type ')' ')'
//////////////////////////////////////////////
  ;
// (declare-type c (τ₁ ⋯ τᵢ)) with i > 0
// is equivalent to
// (declare c (-> τ₁ ⋯ τᵢ type))
//
// (declare-type c ()) with i > 0
// is equivalent to
// (declare c type)
//
// (declare-rule r ν₁ ⋯ νᵢ τ)
// is equivalent to
// (declare r (-> r ν₁ ⋯ νᵢ τ))


iden : ID ;

kind 
  : 'type'
  | '(' '!' iden ntype kind ')' 
// Extension //////////////
  | '(' '->' type+ kind ')' 
  ;
////////////////////////////////
// (-> τ₁ ⋯ τᵢ κ) with i > 1
// is equivalent to
// (-> τ₁ (-> τ₂ ⋯ τᵢ κ))
//
// (-> τ κ)
// is equivalent to
// (-> ξ τ κ) for some fresh ξ

ntype 
  : '(' '^' code term ')'
  | type
// Extension ///////////////////
  | '(' 'provided' code term ')'
////////////////////////////////
  ;
//
// (provided c t)
// is equivalent to
// (! ξ (^ c t)) for some fresh ξ

// Extension /////////////////////
vtype : '(' 'var' iden ntype ')' ;
//////////////////////////////////

type 
  : iden
  | 'mpz'
  | 'mpq'
  | '(' type term+ ')'
  | '(' '!' iden ntype type ')' 
// Extension ///////////////////////////////
  | '(' '->' ( ntype | vtype )+ type ')'  
////////////////////////////////////////////
  ;
// (-> ν₁ ⋯ νᵢ τ) with i > 1
// is equivalent to
// (-> ν₁ (-> ν₂ ⋯ νᵢ τ))
//
// (-> ν τ) with ν in ntype
// is equivalent to
// (-> (var ξ ν) τ) for some fresh ξ
//
// (-> (var ξ ν) τ)
// is equivalent to
// (! ξ ν τ)

typed_par : '(' iden type ')' ;

term
  : iden
  | '_'
  | int_const
  | rat_const
  | '(' term term+ ')'
  | '(' '\\' iden term ')'
  | '(' '@' iden term ')'
  | '(' ':' type term ')'
  | '(' '%' iden type term ')'
// Extensions ///////////////////
  | '(' 'let' iden term term ')'
  | '(' 'has-proof' type term ')'
  | '(' 'assuming' vtype+ term ')'
  ;
//////////////////////////////////
// (let ξ t)
// is equivalent to
// (@ ξ t)
//
// (has-proof τ t)
// is equivalent to
// (: τ t)
//
// (assuming τ₁ ⋯ τᵢ t) with i > 1
// is equivalent to
// (assuming τ₁ (assuming τ₂ ⋯ τᵢ t))
//
// (assuming (var ξ τ) t)
// is equivalent to
// (assuming ξ τ t)

int_const
  : INT
  | '(' '~' INT ')'
  ;

rat_const
  : RAT
  | '(' '~' RAT ')'
   ;

code 
  : INT
/* More */
  ;



INT     : [0-9]+ ;
RAT     : ('0' | [1-9][0-9]*) '/' [1-9][0-9]* ;
ID : [a-zA-Z_][a-zA-Z0-9_']* ;